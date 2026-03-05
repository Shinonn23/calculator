#pragma once

#include "algebra/linear/simplify.hpp"
#include "algebra/matrix/matrix_solver.hpp"
#include "algebra/polynomial/ast_to_poly.hpp"
#include "algebra/polynomial/factor.hpp"
#include "algebra/solver/poly_solver.hpp"
#include "algebra/solver/solver.hpp"
#include "ast/command/history_entry.hpp"
#include "ast/command/math_command.hpp"
#include "config/config.hpp"
#include "core/fraction.hpp"
#include "diagnostics/diagnostic.hpp"
#include "diagnostics/kinds/command_errors.hpp"
#include "diagnostics/sink.hpp"
#include "eval/evaluator.hpp"
#include "parser/math/math_parser.hpp"
#include "runtime/context/context.hpp"
#include "ui/color.hpp"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <optional>
#include <set>
#include <sstream>

namespace math_solver {
    namespace handlers {

        // ── Helpers ──────────────────────────────────────────────────────────

        // Splits a payload on ';', trims whitespace, and discards empty parts.
        // Returns a vector of non-empty equation strings.
        inline std::vector<std::string>
        split_equations(const std::string& payload) {
            std::vector<std::string> parts;
            std::istringstream       ss(payload);
            std::string              tok;
            while (std::getline(ss, tok, ';')) {
                // Trim leading/trailing whitespace.
                size_t a = tok.find_first_not_of(" \t\r\n");
                size_t b = tok.find_last_not_of(" \t\r\n");
                if (a == std::string::npos)
                    continue; // empty after trim
                parts.push_back(tok.substr(a, b - a + 1));
            }
            return parts;
        }

        // Format a double for display: strip trailing zeros.
        inline std::string fmt_double(double v) {
            std::string s   = std::to_string(v);
            size_t      dot = s.find('.');
            if (dot != std::string::npos) {
                s.erase(s.find_last_not_of('0') + 1);
                if (s.back() == '.')
                    s.pop_back();
            }
            return s;
        }

        // Print the augmented matrix [A|b] to sink.
        inline void print_matrix(const std::vector<LinearForm>&  forms,
                                 const std::vector<std::string>& var_order,
                                 DiagnosticSink&                 sink) {
            const size_t                          n    = var_order.size();
            // Build string cells for all entries.
            // Each row: n coefficient cells + 1 RHS cell.
            const size_t                          cols = n + 1;
            std::vector<std::vector<std::string>> cells(
                forms.size(), std::vector<std::string>(cols));
            for (size_t i = 0; i < forms.size(); ++i) {
                for (size_t j = 0; j < n; ++j)
                    cells[i][j] = fmt_double(forms[i].get_coeff(var_order[j]));
                cells[i][n] = fmt_double(-forms[i].constant);
            }
            // Compute column widths.
            std::vector<size_t> widths(cols, 1);
            for (size_t j = 0; j < cols; ++j)
                for (size_t i = 0; i < forms.size(); ++i)
                    widths[j] = std::max(widths[j], cells[i][j].size());

            std::ostringstream oss;
            oss << "  [ A | b ] =\n";
            for (size_t i = 0; i < forms.size(); ++i) {
                oss << "  [ ";
                for (size_t j = 0; j < n; ++j) {
                    oss << std::setw(static_cast<int>(widths[j]))
                        << cells[i][j];
                    if (j + 1 < n)
                        oss << "  ";
                }
                oss << "  |  " << std::setw(static_cast<int>(widths[n]))
                    << cells[i][n] << " ]\n";
            }
            sink.push_output(oss.str());
        }

        // Solve a system of equations given as a ';'-separated payload.
        // Called from do_solve() when more than one equation is detected.
        inline HistoryStatus
        do_solve_system(const std::string&              payload,
                        const std::vector<std::string>& eq_strs,
                        const MathCommand& cmd, Context&    ctx,
                        Config& /*config*/, DiagnosticSink& sink) {
            // 1. Parse each equation and collect its LinearForm (lhs − rhs).
            std::vector<LinearForm> forms;
            std::set<std::string>   all_vars_set;

            for (const auto& eq_str : eq_strs) {
                Parser parser(eq_str);
                auto   pr = parser.parse_equation().with_location(
                    cmd.source_file(), cmd.source_line());
                if (!pr) {
                    sink.push(pr.error());
                    return HistoryStatus::Error;
                }
                auto&           eq = *pr;

                LinearCollector lc(nullptr, eq_str, true); // isolated
                auto            lhs_r = lc.collect(eq->lhs());
                auto            rhs_r = lc.collect(eq->rhs());
                if (!lhs_r || !rhs_r) {
                    // Retry with context.
                    LinearCollector lc2(&ctx, eq_str, false);
                    lhs_r = lc2.collect(eq->lhs());
                    rhs_r = lc2.collect(eq->rhs());
                    if (!lhs_r || !rhs_r) {
                        sink.push(!lhs_r ? lhs_r.error() : rhs_r.error());
                        return HistoryStatus::Error;
                    }
                }

                LinearForm form = *lhs_r - *rhs_r;
                form.simplify();
                for (const auto& var : form.variables())
                    all_vars_set.insert(var);
                forms.push_back(std::move(form));
            }

            if (forms.empty()) {
                sink.push_output("  Usage: :solve <eq1>; <eq2>; ...\n");
                return HistoryStatus::Error;
            }

            // 2. Determine variable order.
            std::vector<std::string> var_order;
            if (!cmd.specific_vars().empty()) {
                var_order = cmd.specific_vars();
            } else {
                var_order.assign(all_vars_set.begin(), all_vars_set.end());
                std::sort(var_order.begin(), var_order.end());
            }

            // 3. Optionally print matrix.
            if (cmd.show_matrix())
                print_matrix(forms, var_order, sink);

            // 4. Solve.
            SolveSystemOptions opts;
            opts.method    = cmd.method();
            opts.free_vars = cmd.free_vars();

            MatrixSolver ms(payload);
            auto         result_r = ms.solve(forms, var_order, opts);
            if (!result_r) {
                sink.push(result_r.error().with_location(cmd.source_file(),
                                                         cmd.source_line()));
                return HistoryStatus::Error;
            }
            const SystemSolveResult& result = *result_r;

            // 5. Rank display.
            if (cmd.show_rank()) {
                std::ostringstream oss;
                oss << "  rank(A) = " << result.rank_A
                    << ", rank([A|b]) = " << result.rank_Ab << "\n";
                sink.push_output(oss.str());
            }

            // 6. Singularity warning.
            if (cmd.detect_singular() && result.smallest_pivot < 1e-6 &&
                result.smallest_pivot < 1e17) {
                Diagnostic w = Diagnostic::warning(
                    "smallest pivot is " + fmt_double(result.smallest_pivot) +
                    " (system may be near-singular)");
                sink.push(w);
            }

            // 7. Output solutions.
            bool as_frac = cmd.as_fraction(); // --exact or --fraction alias
            if (result.is_unique) {
                for (const auto& var : var_order) {
                    auto it = result.solutions.find(var);
                    if (it == result.solutions.end())
                        continue;
                    double             val = it->second;
                    std::ostringstream oss;
                    oss << "  " << var << " = ";
                    if (as_frac) {
                        Fraction frac = double_to_fraction(val);
                        oss << frac.to_string();
                    } else {
                        oss << fmt_double(val);
                    }
                    if (!cmd.no_save()) {
                        ctx.set(var, val);
                        oss << ansi::dim << " (saved)" << ansi::reset;
                    } else {
                        oss << ansi::dim << " (not saved)" << ansi::reset;
                    }
                    oss << "\n";
                    sink.push_output(oss.str());
                }
            } else {
                // Parameterised (free-vars) output.
                std::ostringstream oss;
                oss << "  Free variables:";
                for (const auto& fv : result.free_vars)
                    oss << " " << fv;
                oss << "\n";
                for (const auto& var : var_order) {
                    auto it = result.free_params.find(var);
                    if (it != result.free_params.end())
                        oss << "  " << var << " = " << it->second << "\n";
                }
                sink.push_output(oss.str());
            }

            return HistoryStatus::Success;
        }

        // ── Polynomial dispatch
        // ───────────────────────────────────────────────

        // Format a double for fraction output (reuse fmt_double for now).
        inline std::string fmt_val(double v, bool as_frac) {
            if (as_frac) {
                Fraction frac = double_to_fraction(v);
                return frac.to_string();
            }
            return fmt_double(v);
        }

        // Try to solve a single equation as a univariate polynomial.
        // Returns true and fills diagnostics/context on success or definitive
        // error (no real roots). Returns false if the equation is not a
        // univariate polynomial (caller should fall through to linear path).
        //
        // On success the solved variable is saved to context (unless
        // cmd.no_save()).
        // On no-real-solutions an error is pushed and HistoryStatus::Error is
        // returned wrapped in an optional.
        // If the equation cannot be handled here, returns std::nullopt so the
        // caller falls through to the linear path.
        inline std::optional<HistoryStatus>
        try_poly_solve(const Equation& eq, const std::string& payload,
                       const MathCommand& cmd, Context& ctx,
                       DiagnosticSink& sink) {
            // Try to convert both sides to polynomials.
            ASTToPolynomial conv(payload);
            auto            lhs_r = conv.convert(eq.lhs());
            if (!lhs_r)
                return std::nullopt; // non-polynomial LHS — linear fallback

            ASTToPolynomial conv2(payload);
            auto            rhs_r = conv2.convert(eq.rhs());
            if (!rhs_r)
                return std::nullopt; // non-polynomial RHS — linear fallback

            Polynomial combined = *lhs_r - *rhs_r;

            // Only handle univariate polynomial here.
            if (!combined.is_univariate())
                return std::nullopt; // multivariate — linear solver may handle

            // Only route to polynomial solver if the degree > 1; for degree <=
            // 1 the linear solver is exact and avoids floating-point rounding.
            if (combined.degree() <= 1)
                return std::nullopt;

            std::string      var = combined.single_variable();

            PolynomialSolver ps;
            auto             roots_r = ps.solve(combined, payload);

            if (!roots_r) {
                sink.push(roots_r.error().with_location(cmd.source_file(),
                                                        cmd.source_line()));
                return HistoryStatus::Error;
            }

            const PolyRoots& roots = *roots_r;

            // Warn about discarded complex roots.
            if (roots.complex_count > 0) {
                std::string cnt = std::to_string(roots.complex_count);
                Diagnostic  w   = Diagnostic::warning(
                    cnt + " complex root(s) have no real value and were "
                             "discarded");
                sink.push(w);
            }

            bool               as_frac = cmd.as_fraction();

            // Build output.
            std::ostringstream oss;

            if (roots.real_roots.size() == 1) {
                // Single root (possibly repeated).
                double val  = roots.real_roots[0];
                int    mult = roots.multiplicities[0];

                oss << "  " << var << " = " << fmt_val(val, as_frac);
                if (mult > 1)
                    oss << " (multiplicity " << mult << ")";

                if (!cmd.no_save()) {
                    ctx.set(var, val);
                    oss << ansi::dim << " (saved)" << ansi::reset;
                } else {
                    oss << ansi::dim << " (not saved)" << ansi::reset;
                }
                oss << "\n";
            } else {
                // Multiple distinct roots — store as array.
                std::ostringstream arr_oss;
                arr_oss << "[";
                for (size_t i = 0; i < roots.real_roots.size(); ++i) {
                    if (i)
                        arr_oss << ", ";
                    arr_oss << fmt_val(roots.real_roots[i], as_frac);
                }
                arr_oss << "]";

                oss << "  " << var << " = " << arr_oss.str();

                if (!cmd.no_save()) {
                    ctx.set(var, std::vector<double>(roots.real_roots.begin(),
                                                     roots.real_roots.end()));
                    oss << ansi::dim << " (saved)" << ansi::reset;
                } else {
                    oss << ansi::dim << " (not saved)" << ansi::reset;
                }

                // Annotate multiplicities > 1.
                bool any_mult = false;
                for (int m : roots.multiplicities)
                    if (m > 1) {
                        any_mult = true;
                        break;
                    }
                if (any_mult) {
                    oss << "\n  ";
                    for (size_t i = 0; i < roots.real_roots.size(); ++i) {
                        if (roots.multiplicities[i] > 1)
                            oss << "  " << var << "[" << i << "] multiplicity "
                                << roots.multiplicities[i];
                    }
                }

                oss << "\n";
            }

            // Note numerical method for degree >= 3.
            if (combined.degree() >= 3)
                oss << ansi::dim << "  (solved via Durand-Kerner)"
                    << ansi::reset << "\n";

            sink.push_output(oss.str());
            return HistoryStatus::Success;
        }

        // ── Single-equation solve entry point
        // ─────────────────────────────────

        // Entry point for equation solving.
        // - Attempts to infer the set of unknowns by comparing LHS and RHS
        // variables.
        // - If only one unknown, restricts the solving context to avoid
        // accidental shadowing.
        // - Assumes payload is a valid equation; errors are surfaced via
        // diagnostics.
        // - Side effect: updates the context with the solved variable.
        // - Invariant: result.variable is always set on success.
        inline HistoryStatus do_solve(const std::string& payload,
                                      const MathCommand& cmd, Context& ctx,
                                      Config& config, DiagnosticSink& sink) {
            if (payload.empty()) {
                sink.push_output("  Usage: :solve <lhs> = <rhs>\n");
                return HistoryStatus::Error;
            }

            // Multi-equation dispatch: split on ';' and route to system solver.
            auto equations = split_equations(payload);
            if (equations.size() > 1)
                return do_solve_system(payload, equations, cmd, ctx, config,
                                       sink);
            try {
                Parser parser(payload);
                auto   parse_result = parser.parse_equation().with_location(
                    cmd.source_file(), cmd.source_line());
                if (!parse_result) {
                    sink.push(parse_result.error());
                    return HistoryStatus::Error;
                }
                auto eq = std::move(*parse_result);

                // ── Polynomial path ────────────────────────────────────────
                // Try to solve as a univariate polynomial (degree > 1).
                // Falls through to linear path if not applicable.
                if (auto poly_status =
                        try_poly_solve(*eq, payload, cmd, ctx, sink)) {
                    return *poly_status;
                }
                // ──────────────────────────────────────────────────────────

                std::set<std::string> unknowns;

                try {
                    LinearCollector lc(&ctx, payload, false);
                    auto            lhs_r = lc.collect(eq->lhs());
                    auto            rhs_r = lc.collect(eq->rhs());
                    if (lhs_r && rhs_r)
                        unknowns = ((*lhs_r) - (*rhs_r)).variables();
                } catch (const std::exception& e) {
                    Diagnostic d = Diagnostic::make(e.what(), "E0200", Span{},
                                                    payload, "operation failed")
                                       .with_location(cmd.source_file(),
                                                      cmd.source_line());
                    sink.push(d);
                }

                if (unknowns.empty()) {
                    // Fallback: contextless collection, e.g. for malformed or
                    // incomplete input.
                    LinearCollector lc(nullptr, payload, true);
                    auto            lhs_r = lc.collect(eq->lhs());
                    auto            rhs_r = lc.collect(eq->rhs());
                    if (lhs_r && rhs_r)
                        unknowns = ((*lhs_r) - (*rhs_r)).variables();
                }

                const Context* solve_ctx = &ctx;
                Context        temp_ctx;
                if (unknowns.size() == 1) {
                    // If the target variable is already present in the context,
                    // restrict the solving context to avoid interference from
                    // unrelated bindings.
                    const std::string& target = *unknowns.begin();
                    if (ctx.has(target)) {
                        for (const auto& [name, expr] : ctx.all())
                            if (name != target)
                                temp_ctx.set(name, *expr);
                        solve_ctx = &temp_ctx;
                    }
                }

                EquationSolver solver(solve_ctx, payload);
                auto           result_r = solver.solve(*eq);
                if (!result_r) {
                    sink.push(result_r.error().with_location(
                        cmd.source_file(), cmd.source_line()));
                    return HistoryStatus::Error;
                }
                SolveResult        result = *result_r;

                std::ostringstream oss;

                if (!cmd.no_save()) {
                    ctx.set(result.variable, result.value);
                    oss << "  " << result.variable << " = " << result.value
                        << ansi::dim << " (saved)" << ansi::reset << "\n";
                } else {
                    oss << "  " << result.variable << " = " << result.value
                        << ansi::dim << " (not saved)" << ansi::reset << "\n";
                }

                sink.push_output(oss.str());
                return HistoryStatus::Success;

            } catch (const std::exception& e) {
                Diagnostic d =
                    Diagnostic::make(e.what(), "E0200", Span{}, payload,
                                     "operation failed")
                        .with_location(cmd.source_file(), cmd.source_line());
                sink.push(d);
                return HistoryStatus::Error;
            }
        }

        // Simplification entry point.
        // - Handles both equations and expressions, with options for variable
        // ordering, isolation, and fraction output.
        // - Warnings are surfaced directly to the user; these may indicate
        // domain-level issues (e.g., loss of generality).
        // - Returns Warning status if any warnings are emitted, else Success.
        // - Invariant: result.canonical is always printable.
        inline HistoryStatus do_simplify(const std::string& payload,
                                         const MathCommand& cmd, Context& ctx,
                                         Config& config, DiagnosticSink& sink) {
            if (payload.empty()) {
                sink.push_output("  Usage: :simplify <lhs> = <rhs> [--vars x "
                                 "y] [--isolated] [--fraction]\n");
                return HistoryStatus::Error;
            }
            try {
                Parser parser(payload);
                auto   parse_result = parser.parse_equation().with_location(
                    cmd.source_file(), cmd.source_line());
                if (!parse_result) {
                    sink.push(parse_result.error());
                    return HistoryStatus::Error;
                }
                auto            eq = std::move(*parse_result);
                SimplifyOptions opts;
                opts.var_order = cmd.specific_vars();
                opts.isolated  = cmd.isolated();
                opts.as_fraction =
                    cmd.as_fraction() || config.settings().output_fraction;

                Simplifier     simplifier(&ctx, payload);
                SimplifyResult result      = simplifier.simplify(*eq, opts);
                bool           has_warning = false;

                for (const auto& w : result.warnings) {
                    Diagnostic d = Diagnostic::warning(w).with_location(
                        cmd.source_file(), cmd.source_line());
                    sink.push(d);
                    has_warning = true;
                }

                std::ostringstream oss;
                oss << "  " << result.canonical;
                if (result.is_no_solution())
                    oss << "\n  => " << ansi::red << "no solution"
                        << ansi::reset;
                else if (result.is_infinite_solutions())
                    oss << "\n  => " << ansi::green << "infinite solutions"
                        << ansi::reset;
                oss << "\n";
                sink.push_output(oss.str());

                return has_warning ? HistoryStatus::Warning
                                   : HistoryStatus::Success;

            } catch (const std::exception& e) {
                Diagnostic d =
                    Diagnostic::make(e.what(), "E0200")
                        .with_location(cmd.source_file(), cmd.source_line());
                sink.push(d);
                return HistoryStatus::Error;
            }
        }

        // Expands a polynomial expression.
        // - Assumes input is a valid expression; errors are surfaced via
        // diagnostics.
        // - No side effects on context.
        inline HistoryStatus do_expand(const std::string& payload,
                                       const MathCommand& cmd, Context& ctx,
                                       DiagnosticSink& sink) {
            (void)ctx;
            if (payload.empty()) {
                sink.push_output("  Usage: :expand <expr>\n");
                return HistoryStatus::Error;
            }
            try {
                Parser parser(payload);
                auto   parse_result = parser.parse().with_location(
                    cmd.source_file(), cmd.source_line());
                if (!parse_result) {
                    sink.push(parse_result.error());
                    return HistoryStatus::Error;
                }
                auto expr   = std::move(*parse_result);
                auto poly_r = ASTToPolynomial(payload).convert(*expr);
                if (!poly_r) {
                    sink.push(poly_r.error().with_location(cmd.source_file(),
                                                           cmd.source_line()));
                    return HistoryStatus::Error;
                }
                Polynomial poly = *poly_r;
                sink.push_output("  " + poly.to_string() + "\n");
                return HistoryStatus::Success;
            } catch (const std::exception& e) {
                Diagnostic d =
                    Diagnostic::make(e.what(), "E0200")
                        .with_location(cmd.source_file(), cmd.source_line());
                sink.push(d);
                return HistoryStatus::Error;
            }
        }

        // Factors a polynomial expression.
        // - Assumes input is a valid expression; errors are surfaced via
        // diagnostics.
        // - No side effects on context.
        // - Performance: factorization may be expensive for high-degree
        // polynomials.
        inline HistoryStatus do_factor(const std::string& payload,
                                       const MathCommand& cmd, Context& ctx,
                                       DiagnosticSink& sink) {
            (void)ctx;
            if (payload.empty()) {
                sink.push_output("  Usage: :factor <expr>\n");
                return HistoryStatus::Error;
            }
            try {
                Parser parser(payload);
                auto   parse_result = parser.parse().with_location(
                    cmd.source_file(), cmd.source_line());
                if (!parse_result) {
                    sink.push(parse_result.error());
                    return HistoryStatus::Error;
                }
                auto expr   = std::move(*parse_result);
                auto poly_r = ASTToPolynomial(payload).convert(*expr);
                if (!poly_r) {
                    sink.push(poly_r.error().with_location(cmd.source_file(),
                                                           cmd.source_line()));
                    return HistoryStatus::Error;
                }
                auto poly     = *poly_r;
                auto factored = factor_polynomial(poly);
                sink.push_output("  " + factored.to_string() + "\n");
                return HistoryStatus::Success;
            } catch (const std::exception& e) {
                Diagnostic d =
                    Diagnostic::make(e.what(), "E0200")
                        .with_location(cmd.source_file(), cmd.source_line());
                sink.push(d);
                return HistoryStatus::Error;
            }
        }

        // Evaluates an expression or equation.
        // - For equations, checks for approximate equality using
        //   config.settings().solver_tolerance.
        // - For expressions, prints the evaluated result.
        // - No context mutation.
        // - Handles runtime exceptions explicitly to avoid silent failures.
        inline HistoryStatus do_evaluate(const std::string& payload,
                                         const MathCommand& cmd, Context& ctx,
                                         Config& config, DiagnosticSink& sink) {
            if (payload.empty())
                return HistoryStatus::Error;
            try {
                Parser parser(payload);
                auto   parse_result =
                    parser.parse_expression_or_equation().with_location(
                        cmd.source_file(), cmd.source_line());
                if (!parse_result) {
                    sink.push(parse_result.error());
                    return HistoryStatus::Error;
                }
                auto [expr, eq] = std::move(*parse_result);

                if (eq) {
                    Evaluator eval(&ctx, payload, &sink);
                    size_t    err_count = sink.error_count();
                    double    lhs       = eval.evaluate(eq->lhs());
                    double    rhs       = eval.evaluate(eq->rhs());
                    if (sink.error_count() > err_count)
                        return HistoryStatus::Error;
                    bool ok = std::abs(lhs - rhs) <
                              config.settings().solver_tolerance;
                    std::ostringstream oss;
                    oss << "  " << lhs << " = " << rhs << "  "
                        << (ok ? ansi::green : ansi::red)
                        << (ok ? "(true)" : "(false)") << ansi::reset << "\n";
                    sink.push_output(oss.str());
                } else {
                    // Check whether any variable in the expression is
                    // array-bound; if so, take the broadcast path.
                    Evaluator eval(&ctx, payload, &sink);
                    size_t    err_count = sink.error_count();

                    auto      results   = eval.evaluate_broadcast(*expr, ctx);

                    if (sink.error_count() > err_count)
                        return HistoryStatus::Error;

                    std::ostringstream oss;
                    if (results.size() == 1) {
                        oss << "  = " << fmt_double(results[0]) << "\n";
                    } else if (results.size() > 1) {
                        oss << "  = [";
                        for (size_t i = 0; i < results.size(); ++i) {
                            if (i)
                                oss << ", ";
                            oss << fmt_double(results[i]);
                        }
                        oss << "]\n";
                    } else {
                        // No array variables — normal scalar eval.
                        double val = eval.evaluate(*expr);
                        if (sink.error_count() > err_count)
                            return HistoryStatus::Error;
                        oss << "  = " << fmt_double(val) << "\n";
                    }
                    sink.push_output(oss.str());
                }
                return HistoryStatus::Success;
            } catch (const std::exception& e) {
                Diagnostic d =
                    Diagnostic::make(e.what(), "E0200")
                        .with_location(cmd.source_file(), cmd.source_line());
                sink.push(d);
                return HistoryStatus::Error;
            }
        }

        // Dispatches to the appropriate math handler based on command type.
        // - Unknown commands are surfaced with diagnostics.
        // - Invariant: returns a valid HistoryStatus for all cases.
        inline HistoryStatus handle_math(const MathCommand& cmd, Context& ctx,
                                         Config& config, DiagnosticSink& sink) {
            const std::string& payload = cmd.payload();
            switch (cmd.type()) {
            case MathCommand::Type::Solve:
                return do_solve(payload, cmd, ctx, config, sink);
            case MathCommand::Type::Simplify:
                return do_simplify(payload, cmd, ctx, config, sink);
            case MathCommand::Type::Expand:
                return do_expand(payload, cmd, ctx, sink);
            case MathCommand::Type::Factor:
                return do_factor(payload, cmd, ctx, sink);
            case MathCommand::Type::Evaluate:
                return do_evaluate(payload, cmd, ctx, config, sink);
            case MathCommand::Type::Unknown: {
                std::string raw = cmd.raw_command();
                std::string bad = raw.substr(0, raw.find(' '));
                Diagnostic  d   = errors::unknown_command(
                    bad, find_token_span(raw, bad), raw);
                d = d.with_location(cmd.source_file(), cmd.source_line());
                sink.push(d);
            }
                return HistoryStatus::Error;
            }
            return HistoryStatus::Unknown;
        }

    } // namespace handlers
} // namespace math_solver