#pragma once

#include "algebra/linear/simplify.hpp"
#include "algebra/polynomial/ast_to_poly.hpp"
#include "algebra/polynomial/factor.hpp"
#include "algebra/solver/solver.hpp"
#include "ast/command/history_entry.hpp"
#include "ast/command/math_command.hpp"
#include "config/config.hpp"
#include "diagnostics/diagnostic.hpp"
#include "diagnostics/kinds/command_errors.hpp"
#include "diagnostics/sink.hpp"
#include "eval/evaluator.hpp"
#include "parser/math/math_parser.hpp"
#include "runtime/context/context.hpp"
#include "ui/color.hpp"

#include <iostream>

namespace math_solver {
    namespace handlers {

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
                                      Config& /*config*/,
                                      DiagnosticSink& sink) {
            if (payload.empty()) {
                (void)ctx;
                sink.push_output("  Usage: :solve <lhs> = <rhs>\n");
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
                auto                  eq = std::move(*parse_result);
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
                SolveResult result = *result_r;

                ctx.set(result.variable, result.value);
                std::ostringstream oss;
                oss << "  " << result.variable << " = " << result.value
                    << ansi::dim << " (saved)" << ansi::reset << "\n";
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
                (void)ctx;
                (void)config;
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
            if (payload.empty()) {
                (void)ctx;
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
            if (payload.empty()) {
                (void)ctx;
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
        // - For equations, checks for approximate equality (tolerance 1e-12).
        // - For expressions, prints the evaluated result.
        // - No context mutation.
        // - Handles runtime exceptions explicitly to avoid silent failures.
        inline HistoryStatus do_evaluate(const std::string& payload,
                                         const MathCommand& cmd, Context& ctx,
                                         Config& /*config*/,
                                         DiagnosticSink& sink) {
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
                    bool               ok = std::abs(lhs - rhs) < 1e-12;
                    std::ostringstream oss;
                    oss << "  " << lhs << " = " << rhs << "  "
                        << (ok ? ansi::green : ansi::red)
                        << (ok ? "(true)" : "(false)") << ansi::reset << "\n";
                    sink.push_output(oss.str());
                } else {
                    Evaluator eval(&ctx, payload, &sink);
                    size_t    err_count = sink.error_count();
                    double    val       = eval.evaluate(*expr);
                    if (sink.error_count() > err_count)
                        return HistoryStatus::Error;
                    std::ostringstream oss;
                    oss << "  = " << val << "\n";
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