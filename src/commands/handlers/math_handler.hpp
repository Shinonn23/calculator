#pragma once

#include "algebra/linear/simplify.hpp"
#include "algebra/polynomial/ast_to_poly.hpp"
#include "algebra/polynomial/factor.hpp"
#include "algebra/solver/solver.hpp"
#include "ast/command/history_entry.hpp"
#include "ast/command/math_command.hpp"
#include "commands/handlers/diagnostics/math_diag.hpp"
#include "config/config.hpp"
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
        inline HistoryStatus do_solve(const std::string& payload, Context& ctx,
                                      Config& /*config*/) {
            if (payload.empty()) {
                diag::emit_usage(":solve <lhs> = <rhs>");
                return HistoryStatus::Error;
            }
            try {
                Parser                parser(payload);
                auto                  eq = parser.parse_equation();
                std::set<std::string> unknowns;

                try {
                    LinearCollector lc(&ctx, payload, false);
                    unknowns = (lc.collect(eq->lhs()) - lc.collect(eq->rhs()))
                                   .variables();
                } catch (...) {
                    // Fallback: contextless collection, e.g. for malformed or
                    // incomplete input.
                    LinearCollector lc(nullptr, payload, true);
                    unknowns = (lc.collect(eq->lhs()) - lc.collect(eq->rhs()))
                                   .variables();
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
                SolveResult    result = solver.solve(*eq);

                ctx.set(result.variable, result.value);
                std::cout << "  " << result.variable << " = " << result.value
                          << ansi::dim << " (saved)" << ansi::reset << "\n";
                return HistoryStatus::Success;

            } catch (const UndefinedVariableError& e) {
                diag::emit_undefined_var(e, ctx);
                return HistoryStatus::Error;
            } catch (const MathError& e) {
                diag::emit_math_error_with_hint(e);
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
                                         Config& config) {
            if (payload.empty()) {
                diag::emit_usage(":simplify <lhs> = <rhs> "
                                 "[--vars x y] [--isolated] [--fraction]");
                return HistoryStatus::Error;
            }
            try {
                Parser          parser(payload);
                auto            eq = parser.parse_equation();
                SimplifyOptions opts;
                opts.var_order = cmd.specific_vars();
                opts.isolated  = cmd.isolated();
                opts.as_fraction =
                    cmd.as_fraction() || config.settings().output_fraction;

                Simplifier     simplifier(&ctx, payload);
                SimplifyResult result      = simplifier.simplify(*eq, opts);
                bool           has_warning = false;

                for (const auto& w : result.warnings) {
                    std::cout << ansi::yellow << "  Warning: " << ansi::reset
                              << w << "\n";
                    has_warning = true;
                }

                std::cout << "  " << result.canonical;
                if (result.is_no_solution())
                    std::cout << "\n  => " << ansi::red << "no solution"
                              << ansi::reset;
                else if (result.is_infinite_solutions())
                    std::cout << "\n  => " << ansi::green
                              << "infinite solutions" << ansi::reset;
                std::cout << "\n";

                return has_warning ? HistoryStatus::Warning
                                   : HistoryStatus::Success;

            } catch (const UndefinedVariableError& e) {
                diag::emit_undefined_var(e, ctx);
                return HistoryStatus::Error;
            } catch (const MathError& e) {
                diag::emit_math_error(e);
                return HistoryStatus::Error;
            }
        }

        // Expands a polynomial expression.
        // - Assumes input is a valid expression; errors are surfaced via
        // diagnostics.
        // - No side effects on context.
        inline HistoryStatus do_expand(const std::string& payload,
                                       Context&           ctx) {
            if (payload.empty()) {
                diag::emit_usage(":expand <expr>");
                return HistoryStatus::Error;
            }
            try {
                Parser     parser(payload);
                auto       expr = parser.parse();
                Polynomial poly = ASTToPolynomial(payload).convert(*expr);
                std::cout << "  " << poly.to_string() << "\n";
                return HistoryStatus::Success;
            } catch (const UndefinedVariableError& e) {
                diag::emit_undefined_var(e, ctx);
                return HistoryStatus::Error;
            } catch (const MathError& e) {
                diag::emit_math_error(e);
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
                                       Context&           ctx) {
            if (payload.empty()) {
                diag::emit_usage(":factor <expr>");
                return HistoryStatus::Error;
            }
            try {
                Parser parser(payload);
                auto   expr     = parser.parse();
                auto   poly     = ASTToPolynomial(payload).convert(*expr);
                auto   factored = factor_polynomial(poly);
                std::cout << "  " << factored.to_string() << "\n";
                return HistoryStatus::Success;
            } catch (const UndefinedVariableError& e) {
                diag::emit_undefined_var(e, ctx);
                return HistoryStatus::Error;
            } catch (const MathError& e) {
                diag::emit_math_error(e);
                return HistoryStatus::Error;
            }
        }

        // Evaluates an expression or equation.
        // - For equations, checks for approximate equality (tolerance 1e-12).
        // - For expressions, prints the evaluated result.
        // - No context mutation.
        // - Handles runtime exceptions explicitly to avoid silent failures.
        inline HistoryStatus do_evaluate(const std::string& payload,
                                         Context& ctx, Config& /*config*/) {
            if (payload.empty())
                return HistoryStatus::Error;
            try {
                Parser parser(payload);
                auto [expr, eq] = parser.parse_expression_or_equation();

                if (eq) {
                    Evaluator eval(&ctx, payload);
                    double    lhs = eval.evaluate(eq->lhs());
                    double    rhs = eval.evaluate(eq->rhs());
                    bool      ok  = std::abs(lhs - rhs) < 1e-12;
                    std::cout << "  " << lhs << " = " << rhs << "  "
                              << (ok ? ansi::green : ansi::red)
                              << (ok ? "(true)" : "(false)") << ansi::reset
                              << "\n";
                } else {
                    Evaluator eval(&ctx, payload);
                    std::cout << "  = " << eval.evaluate(*expr) << "\n";
                }
                return HistoryStatus::Success;

            } catch (const UndefinedVariableError& e) {
                diag::emit_undefined_var(e, ctx);
                return HistoryStatus::Error;
            } catch (const MathError& e) {
                diag::emit_math_error(e);
                return HistoryStatus::Error;
            } catch (const std::exception& e) {
                diag::emit_runtime_exception(e);
                return HistoryStatus::Error;
            }
        }

        // Dispatches to the appropriate math handler based on command type.
        // - Unknown commands are surfaced with diagnostics.
        // - Invariant: returns a valid HistoryStatus for all cases.
        inline HistoryStatus handle_math(const MathCommand& cmd, Context& ctx,
                                         Config& config) {
            const std::string& payload = cmd.payload();
            switch (cmd.type()) {
            case MathCommand::Type::Solve:
                return do_solve(payload, ctx, config);
            case MathCommand::Type::Simplify:
                return do_simplify(payload, cmd, ctx, config);
            case MathCommand::Type::Expand:
                return do_expand(payload, ctx);
            case MathCommand::Type::Factor:
                return do_factor(payload, ctx);
            case MathCommand::Type::Evaluate:
                return do_evaluate(payload, ctx, config);
            case MathCommand::Type::Unknown:
                diag::emit_unknown_math_command(
                    cmd.raw_command(),
                    cmd.raw_command().substr(0, cmd.raw_command().find(' ')));
                return HistoryStatus::Error;
            }
            return HistoryStatus::Unknown;
        }

    } // namespace handlers
} // namespace math_solver