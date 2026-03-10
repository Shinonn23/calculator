#pragma once

//! # Module — `src/commands/handlers/math_handler.hpp`
//!
//! Implements the math-command handler pipeline: `handle_math` dispatches to
//! `do_evaluate`, `do_solve`, `do_simplify`, `do_expand`, and `do_factor`
//! based on `MathCommand::Type`. The solve path splits on `;` to support
//! multi-equation linear systems via `do_solve_system`, and also attempts
//! polynomial solving via `try_poly_solve` before falling back to the linear
//! solver. All functions are `inline` and live entirely in this header.

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
#include "diagnostics/kinds/solver_errors.hpp"
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

        /// Split `payload` on `';'`, trim whitespace, and discard empty parts.
        ///
        /// # Returns
        ///
        /// A vector of non-empty, whitespace-trimmed equation strings.
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

        /// Lift a Diagnostic from payload-space to command-space.
        ///
        /// Searches for `d.input` as a literal substring in `raw_cmd`. If
        /// found, shifts `d.span` by that byte offset and sets `d.input` to
        /// `raw_cmd` so the full command line is shown in error output. Returns
        /// `d` unchanged if `d.input` is not a substring.
        static inline Diagnostic lift_to_cmd(Diagnostic         d,
                                             const std::string& raw_cmd) {
            if (d.input.empty() || d.input == raw_cmd)
                return d;
            size_t off = raw_cmd.find(d.input);
            if (off == std::string::npos)
                return d;
            d.span.start += off;
            d.span.end += off;
            d.input = raw_cmd;
            return d;
        }

        /// Format `v` as a decimal string with trailing zeros stripped.
        ///
        /// # Returns
        ///
        /// A string representation of `v` with no trailing fractional zeros
        /// and no trailing decimal point.
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

        /// Emit the augmented matrix `[A|b]` for a linear system to `sink`.
        ///
        /// Columns are right-aligned to the widest entry in each column.
        /// The RHS column `b` is separated by ` | `.
        ///
        /// # Arguments
        ///
        /// * `forms`     — One `LinearForm` per equation (LHS − RHS already
        /// combined).
        /// * `var_order` — Variable names in the column order to use for `A`.
        /// * `sink`      — Diagnostic sink that receives the formatted matrix
        /// text.
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

        /// Solve a system of linear equations given as pre-split equation
        /// strings.
        ///
        /// Called from `do_solve` when the payload contains more than one
        /// equation (separated by `';'`). Each string in `eq_strs` is parsed
        /// and collected into a `LinearForm`; the resulting system is solved
        /// via `MatrixSolver`. Solutions are saved to `ctx` unless
        /// `cmd.no_save()` is set.
        ///
        /// # Arguments
        ///
        /// * `payload`  — The original raw payload string (used for
        /// diagnostics).
        /// * `eq_strs`  — Pre-split, trimmed equation strings (at least two).
        /// * `cmd`      — The originating `MathCommand`; supplies flags and
        /// source info.
        /// * `ctx`      — Variable context; mutated when solutions are saved.
        /// * `sink`     — Diagnostic sink for errors, warnings, and output.
        ///
        /// # Returns
        ///
        /// `HistoryStatus::Success` when the system is solved and output
        /// emitted, `HistoryStatus::Error` on parse failure, collection
        /// failure, or solver error.
        ///
        /// # Errors
        ///
        /// Pushes a parse diagnostic (E-series) if any equation string fails to
        /// parse. Pushes a solver diagnostic (E0310/E0311) on singular or
        /// inconsistent systems.
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
                    sink.push(lift_to_cmd(pr.error(), cmd.raw_command()));
                    return HistoryStatus::Error;
                }
                auto&           eq = *pr;

                LinearCollector lc(nullptr, eq_str, true); // isolated
                auto            lhs_r = lc.collect(eq->lhs());
                auto            rhs_r = lc.collect(eq->rhs());
                if (!lhs_r || !rhs_r) {
                    if (cmd.isolated()) {
                        sink.push(
                            lift_to_cmd(!lhs_r ? lhs_r.error() : rhs_r.error(),
                                        cmd.raw_command()));
                        return HistoryStatus::Error;
                    }
                    // Retry with context (non-isolated mode only).
                    LinearCollector lc2(&ctx, eq_str, false);
                    lhs_r = lc2.collect(eq->lhs());
                    rhs_r = lc2.collect(eq->rhs());
                    if (!lhs_r || !rhs_r) {
                        sink.push(
                            lift_to_cmd(!lhs_r ? lhs_r.error() : rhs_r.error(),
                                        cmd.raw_command()));
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
                sink.push(lift_to_cmd(result_r.error().with_location(
                                          cmd.source_file(), cmd.source_line()),
                                      cmd.raw_command()));
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
                    oss << " " << fv << ",";
                // Remove trailing comma
                std::string line = oss.str();
                if (!line.empty() && line.back() == ',')
                    line.pop_back();
                sink.push_output(line + "\n");

                for (const auto& var : var_order) {
                    auto it = result.free_params.find(var);
                    if (it != result.free_params.end() && it->second != var) {
                        std::ostringstream param_oss;
                        param_oss << "  " << var << " = " << it->second << "\n";
                        sink.push_output(param_oss.str());
                    }
                }
            }

            return HistoryStatus::Success;
        }

        // ── Polynomial dispatch
        // ───────────────────────────────────────────────

        /// Format `v` as a decimal or fraction string depending on `as_frac`.
        ///
        /// # Arguments
        ///
        /// * `v`       — The value to format.
        /// * `as_frac` — When `true`, convert via `double_to_fraction`;
        /// otherwise
        ///   delegate to `fmt_double`.
        ///
        /// # Returns
        ///
        /// A human-readable string for `v`.
        inline std::string fmt_val(double v, bool as_frac) {
            if (as_frac) {
                Fraction frac = double_to_fraction(v);
                return frac.to_string();
            }
            return fmt_double(v);
        }

        /// Attempt to solve `eq` as a univariate polynomial of degree > 1.
        ///
        /// Converts both sides to `Polynomial` via `ASTToPolynomial`. If the
        /// combined polynomial is univariate and has degree > 1, it is solved
        /// with `PolynomialSolver`; roots are saved to `ctx` unless
        /// `cmd.no_save()` is set. For degree ≤ 1, or when either side is
        /// non-polynomial or multivariate, the function returns `std::nullopt`
        /// so the caller can fall through to the linear solver.
        ///
        /// # Arguments
        ///
        /// * `eq`      — The parsed equation to attempt.
        /// * `payload` — Raw source string for diagnostic span construction.
        /// * `cmd`     — The originating `MathCommand`; supplies flags and
        /// source info.
        /// * `ctx`     — Variable context; mutated when roots are saved.
        /// * `sink`    — Diagnostic sink for warnings, errors, and output.
        ///
        /// # Returns
        ///
        /// `Some(HistoryStatus::Success)` when polynomial roots are found and
        /// output is emitted. `Some(HistoryStatus::Error)` when the polynomial
        /// is valid but has no real solutions. `std::nullopt` when the
        /// polynomial path does not apply and the caller should try the linear
        /// solver.
        ///
        /// # Errors
        ///
        /// Pushes E0310 (no solution) when the discriminant is negative or all
        /// roots are complex. Pushes a method-ignored warning when `--method`
        /// was set (it has no effect on the polynomial solver).
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

            // Warn if --method was explicitly set — it has no effect here.
            if (cmd.method_explicitly_set()) {
                // Reconstruct the flag string to point span at it.
                std::string method_flag = (cmd.method() == SolveMethod::LU)
                                              ? "--method=lu"
                                              : "--method=gauss";
                sink.push(errors::method_ignored_for_poly(
                    method_flag, cmd.raw_command(), cmd.source_file(),
                    cmd.source_line()));
            }

            std::string      var = combined.single_variable();

            PolynomialSolver ps;
            auto             roots_r = ps.solve(combined, payload);

            if (!roots_r) {
                sink.push(lift_to_cmd(roots_r.error().with_location(
                                          cmd.source_file(), cmd.source_line()),
                                      cmd.raw_command()));
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

        // ── Single-equation solve entry point ────────────────────────────────

        /// Solve a single equation given as `payload` and save the result to
        /// `ctx`.
        ///
        /// Dispatches to `do_solve_system` when `payload` contains `';'`.
        /// Otherwise, parses the payload as an equation, attempts the
        /// polynomial path via `try_poly_solve`, and falls back to
        /// `EquationSolver` (linear). When exactly one unknown is found and it
        /// already exists in `ctx`, a temporary context is used to prevent the
        /// existing binding from interfering with the solve.
        ///
        /// # Arguments
        ///
        /// * `payload` — Raw equation string (e.g. `"2x + 3 = 7"`).
        /// * `cmd`     — The originating `MathCommand`; supplies flags and
        /// source info.
        /// * `ctx`     — Variable context; updated with the solution unless
        /// `cmd.no_save()`.
        /// * `config`  — Configuration store forwarded to `do_solve_system`.
        /// * `sink`    — Diagnostic sink for errors, warnings, and output.
        ///
        /// # Returns
        ///
        /// `HistoryStatus::Success` when a solution is found and output
        /// emitted, `HistoryStatus::Error` on parse or solver failure.
        ///
        /// # Errors
        ///
        /// Pushes parse diagnostics on invalid input. Pushes solver diagnostics
        /// (E0310, E0311, E0315) from the underlying solvers.
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
                    sink.push(
                        lift_to_cmd(parse_result.error(), cmd.raw_command()));
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
                    const Context*  lc_ctx = cmd.isolated() ? nullptr : &ctx;
                    LinearCollector lc(lc_ctx, payload, cmd.isolated());
                    auto            lhs_r = lc.collect(eq->lhs());
                    auto            rhs_r = lc.collect(eq->rhs());
                    if (lhs_r && rhs_r)
                        unknowns = ((*lhs_r) - (*rhs_r)).variables();
                } catch (const std::exception& e) {
                    Diagnostic d = Diagnostic::make(e.what(), "E0200", Span{},
                                                    payload, "operation failed")
                                       .with_location(cmd.source_file(),
                                                      cmd.source_line());
                    sink.push(lift_to_cmd(d, cmd.raw_command()));
                }

                if (unknowns.empty() && !cmd.isolated()) {
                    // Fallback: contextless collection, e.g. for malformed or
                    // incomplete input.
                    LinearCollector lc(nullptr, payload, true);
                    auto            lhs_r = lc.collect(eq->lhs());
                    auto            rhs_r = lc.collect(eq->rhs());
                    if (lhs_r && rhs_r)
                        unknowns = ((*lhs_r) - (*rhs_r)).variables();
                }

                // When --free-vars is set and the equation has multiple
                // unknowns, delegate to the system solver so parameterisation
                // logic is applied (treat the single equation as a 1×n system).
                if (cmd.free_vars() && unknowns.size() > 1) {
                    return do_solve_system(payload, {payload}, cmd, ctx, config,
                                           sink);
                }

                const Context* solve_ctx = cmd.isolated() ? nullptr : &ctx;
                Context        temp_ctx;
                if (!cmd.isolated() && unknowns.size() == 1) {
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
                    sink.push(
                        lift_to_cmd(result_r.error().with_location(
                                        cmd.source_file(), cmd.source_line()),
                                    cmd.raw_command()));
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
                sink.push(lift_to_cmd(d, cmd.raw_command()));
                return HistoryStatus::Error;
            }
        }

        /// Simplify `payload` to canonical `Ax + By = C` form and emit the
        /// result.
        ///
        /// Parses `payload` as an equation, applies `Simplifier` with the
        /// options extracted from `cmd` (variable ordering, isolated mode,
        /// fraction output), and emits the canonical form. If the result is a
        /// tautology or contradiction, an annotation is appended. Any
        /// `SimplifyResult::warnings` are forwarded to `sink` as
        /// `Diagnostic::warning` entries.
        ///
        /// # Arguments
        ///
        /// * `payload` — Raw equation string to simplify.
        /// * `cmd`     — The originating `MathCommand`; supplies `--vars`,
        /// `--isolated`,
        ///   `--fraction`, and source location.
        /// * `ctx`     — Variable context consulted in non-isolated mode.
        /// * `config`  — Configuration; `output_fraction` setting is OR-ed with
        /// `--fraction`.
        /// * `sink`    — Diagnostic sink for warnings, errors, and output.
        ///
        /// # Returns
        ///
        /// `HistoryStatus::Warning` when at least one warning was emitted,
        /// `HistoryStatus::Success` otherwise, `HistoryStatus::Error` on parse
        /// failure.
        ///
        /// # Errors
        ///
        /// Pushes parse diagnostics on invalid input. Pushes E0200 for
        /// unexpected runtime exceptions.
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
                SimplifyResult result = simplifier.simplify(*eq, opts);

                if (result.canonical.empty()) {
                    const std::string& raw = cmd.raw_command();
                    Diagnostic         d   = errors::unsupported_equation(
                        ":simplify only supports linear expressions",
                        find_token_span(raw, payload), raw, cmd.source_file(),
                        cmd.source_line());
                    d.help = "use :solve for polynomial or non-linear equations";
                    sink.push(d);
                    return HistoryStatus::Error;
                }

                bool has_warning = false;
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

        /// Expand `payload` to standard polynomial form and emit the result.
        ///
        /// Parses `payload` as a math expression, converts it to a `Polynomial`
        /// via `ASTToPolynomial`, and emits the expanded form. If the
        /// AST-to-poly conversion fails (e.g. for transcendental expressions),
        /// the evaluator is tried as a numeric fallback.
        ///
        /// # Arguments
        ///
        /// * `payload` — Raw expression string (e.g. `"(x+1)^3"`).
        /// * `cmd`     — The originating `MathCommand`; supplies source
        /// location.
        /// * `ctx`     — Variable context consulted by the numeric fallback
        /// evaluator.
        /// * `sink`    — Diagnostic sink for errors and output.
        ///
        /// # Returns
        ///
        /// `HistoryStatus::Success` when expansion succeeds and output is
        /// emitted, `HistoryStatus::Error` on parse or conversion failure.
        ///
        /// # Errors
        ///
        /// Pushes parse diagnostics on invalid input. Pushes the
        /// `ASTToPolynomial` diagnostic (E0315 or similar) when conversion
        /// fails and numeric evaluation also fails. Pushes E0200 for unexpected
        /// runtime exceptions.
        inline HistoryStatus do_expand(const std::string& payload,
                                       const MathCommand& cmd, Context& ctx,
                                       DiagnosticSink& sink) {
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
                    // Fallback: try numeric evaluation (handles trig, etc.)
                    DiagnosticSink temp_sink;
                    Evaluator      eval(&ctx, payload, &temp_sink);
                    double         val = eval.evaluate(*expr);
                    if (temp_sink.error_count() == 0) {
                        sink.push_output("  " + fmt_double(val) + "\n");
                        return HistoryStatus::Success;
                    }
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

        /// Factorise `payload` and emit the factored polynomial form.
        ///
        /// Parses `payload` as a math expression, converts it to a `Polynomial`
        /// via `ASTToPolynomial`, and applies `factor_polynomial`. Does not
        /// mutate the variable context.
        ///
        /// # Arguments
        ///
        /// * `payload` — Raw expression string to factorise.
        /// * `cmd`     — The originating `MathCommand`; supplies source
        /// location.
        /// * `ctx`     — Variable context (not mutated; passed for future
        /// extension).
        /// * `sink`    — Diagnostic sink for errors and output.
        ///
        /// # Returns
        ///
        /// `HistoryStatus::Success` when factorisation succeeds and output is
        /// emitted, `HistoryStatus::Error` on parse or conversion failure.
        ///
        /// # Errors
        ///
        /// Pushes parse diagnostics on invalid input. Pushes the
        /// `ASTToPolynomial` diagnostic when conversion fails. Pushes E0200 for
        /// unexpected runtime exceptions.
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

        /// Evaluate `payload` as a math expression or equality check.
        ///
        /// Parses `payload` via `Parser::parse_expression_or_equation`. For an
        /// equation, evaluates both sides and compares within
        /// `config.settings().solver_tolerance`, printing `(true)` or
        /// `(false)`. For a plain expression, calls
        /// `Evaluator::evaluate_broadcast`; if any variable is array-bound the
        /// result is a bracketed list, otherwise a scalar. Does not mutate the
        /// variable context.
        ///
        /// # Arguments
        ///
        /// * `payload` — Raw expression or equation string.
        /// * `cmd`     — The originating `MathCommand`; supplies source
        /// location.
        /// * `ctx`     — Variable context consulted during evaluation.
        /// * `config`  — Configuration; `solver_tolerance` used for equality
        /// checks.
        /// * `sink`    — Diagnostic sink for errors and output.
        ///
        /// # Returns
        ///
        /// `HistoryStatus::Success` when evaluation succeeds and output is
        /// emitted, `HistoryStatus::Error` on parse or evaluation failure.
        ///
        /// # Errors
        ///
        /// Pushes parse diagnostics on invalid input. Pushes evaluator
        /// diagnostics (variable-not-found, division-by-zero, etc.) from
        /// `Evaluator`. Pushes E0200 for unexpected runtime exceptions.
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
                    eval.set_source(cmd.source_file(), cmd.source_line());
                    size_t err_count = sink.error_count();

                    auto   results   = eval.evaluate_broadcast(*expr, ctx);

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

        /// Dispatch a `MathCommand` to the appropriate sub-handler and return
        /// its status.
        ///
        /// Routes `cmd.type()` to `do_solve`, `do_simplify`, `do_expand`,
        /// `do_factor`, or `do_evaluate`. For `MathCommand::Type::Unknown`,
        /// emits an `unknown_command` diagnostic.
        ///
        /// # Arguments
        ///
        /// * `cmd`    — The math command to execute.
        /// * `ctx`    — Variable context forwarded to the selected sub-handler.
        /// * `config` — Configuration forwarded to the selected sub-handler.
        /// * `sink`   — Diagnostic sink for errors and output.
        ///
        /// # Returns
        ///
        /// The `HistoryStatus` produced by the selected sub-handler, or
        /// `HistoryStatus::Error` for `Unknown`. Returns
        /// `HistoryStatus::Unknown` only if a new `MathCommand::Type` is added
        /// without a corresponding case.
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
                    bad, find_token_span(raw, bad), raw, cmd.source_file(),
                    cmd.source_line());
                sink.push(d);
            }
                return HistoryStatus::Error;
            }
            return HistoryStatus::Unknown;
        }

    } // namespace handlers
} // namespace math_solver