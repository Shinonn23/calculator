#pragma once

#include "algebra/linear/simplify.hpp"
#include "algebra/polynomial/ast_to_poly.hpp"
#include "algebra/polynomial/factor.hpp"
#include "algebra/solver/solver.hpp"
#include "ast/command/history_entry.hpp"
#include "ast/command/math_command.hpp"
#include "config/config.hpp"
#include "eval/evaluator.hpp"
#include "parser/math/math_parser.hpp"
#include "runtime/context/context.hpp"
#include "ui/color.hpp"
#include "ui/suggestions.hpp"

#include <iostream>

namespace math_solver {
    namespace handlers {

        // Entry point for equation solving.
        // - Attempts to isolate a single unknown if possible, using a temporary
        // context to avoid clobbering unrelated variables.
        // - If multiple unknowns, falls back to using the full context.
        // - Assumes payload is a valid equation string.
        // - On success, updates the context with the solved variable.
        // - Suggests similar variable names on undefined variable errors.
        inline HistoryStatus
        do_solve(const std::string& payload, Context& ctx, Config& /*config*/) {
            if (payload.empty()) {
                std::cout << "  Usage: :solve <lhs> = <rhs>\n";
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
                    // Fallback: contextless collection, disables type-based
                    // inference.
                    LinearCollector lc(nullptr, payload, true);
                    unknowns = (lc.collect(eq->lhs()) - lc.collect(eq->rhs()))
                                   .variables();
                }

                const Context* solve_ctx = &ctx;
                Context        temp_ctx;
                if (unknowns.size() == 1) {
                    // Only isolate the target variable; exclude others from
                    // context to avoid accidental shadowing.
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

            } catch (const UndefinedVariableError& undef_err) {
                // Suggests similar variable names to mitigate user typos.
                auto match = suggest(undef_err.var_name(), ctx.all_names());
                if (match) {
                    undef_err.with_help(
                        "a variable with a similar name exists: `" + *match +
                        "`");
                }
                std::cout << undef_err.format();
                return HistoryStatus::Error;
            } catch (const MathError& e) {
                std::cout << e.format();
                // NonLinearError is surfaced with a hint for variable
                // definition.
                if (dynamic_cast<const NonLinearError*>(&e))
                    std::cout << ansi::dim
                              << "  Hint: use :set to define variables\n"
                              << ansi::reset;
                return HistoryStatus::Error;
            }
        }

        // Canonicalizes and simplifies equations.
        // - Honors variable order and isolation flags from the command and
        // config.
        // - Emits warnings for non-canonical or ambiguous forms.
        // - Returns warning status if any warnings are present.
        inline HistoryStatus do_simplify(const std::string& payload,
                                         const MathCommand& cmd,
                                         Context&           ctx,
                                         Config&            config) {
            if (payload.empty()) {
                std::cout << "  Usage: :simplify <lhs> = <rhs> [-vars x y] "
                             "[-isolated] [-fraction]\n";
                return HistoryStatus::Error;
            }
            try {
                Parser          parser(payload);
                auto            eq = parser.parse_equation();

                SimplifyOptions opts;
                opts.var_order = cmd.specific_vars();
                opts.isolated  = cmd.isolated();
                opts.as_fraction =
                    cmd.as_fraction() || config.settings().fraction_mode;

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

            } catch (const UndefinedVariableError& undef_err) {
                auto match = suggest(undef_err.var_name(), ctx.all_names());
                if (match) {
                    undef_err.with_help(
                        "a variable with a similar name exists: `" + *match +
                        "`");
                }
                std::cout << undef_err.format();
                return HistoryStatus::Error;
            } catch (const MathError& e) {
                std::cout << e.format();
                return HistoryStatus::Error;
            }
        }

        // Expands polynomial expressions.
        // - Assumes input is a valid expression.
        // - Converts AST to polynomial and prints expanded form.
        // - No fallback for non-polynomial input; errors are surfaced.
        inline HistoryStatus do_expand(const std::string& payload,
                                       Context&           ctx) {
            if (payload.empty()) {
                std::cout << "  Usage: :expand <expr>\n";
                return HistoryStatus::Error;
            }
            try {
                Parser     parser(payload);
                auto       expr = parser.parse();
                Polynomial poly = ASTToPolynomial(payload).convert(*expr);
                std::cout << "  " << poly.to_string() << "\n";
                return HistoryStatus::Success;
            } catch (const UndefinedVariableError& undef_err) {
                auto match = suggest(undef_err.var_name(), ctx.all_names());
                if (match) {
                    undef_err.with_help(
                        "a variable with a similar name exists: `" + *match +
                        "`");
                }
                std::cout << undef_err.format();
                return HistoryStatus::Error;
            } catch (const MathError& e) {
                std::cout << e.format();
                return HistoryStatus::Error;
            }
        }

        // Factors polynomial expressions.
        // - Only operates on valid polynomial input; errors otherwise.
        // - Returns factored form as a string.
        inline HistoryStatus do_factor(const std::string& payload,
                                       Context&           ctx) {
            if (payload.empty()) {
                std::cout << "  Usage: :factor <expr>\n";
                return HistoryStatus::Error;
            }
            try {
                Parser parser(payload);
                auto   expr     = parser.parse();
                auto   poly     = ASTToPolynomial(payload).convert(*expr);
                auto   factored = factor_polynomial(poly);
                std::cout << "  " << factored.to_string() << "\n";
                return HistoryStatus::Success;
            } catch (const UndefinedVariableError& undef_err) {
                auto match = suggest(undef_err.var_name(), ctx.all_names());
                if (match) {
                    undef_err.with_help(
                        "a variable with a similar name exists: `" + *match +
                        "`");
                }
                std::cout << undef_err.format();
                return HistoryStatus::Error;
            } catch (const MathError& e) {
                std::cout << e.format();
                return HistoryStatus::Error;
            }
        }

        // Evaluates expressions or equations numerically.
        // - For equations, checks for approximate equality within a tight
        // epsilon.
        // - For expressions, prints the evaluated value.
        // - No fallback expansion; errors are surfaced directly.
        // - Catches std::exception as a last resort to avoid process abort.
        inline HistoryStatus do_evaluate(const std::string& payload,
                                         Context&           ctx,
                                         Config& /*config*/) {
            if (payload.empty())
                return HistoryStatus::Error;
            try {
                Parser parser(payload);
                auto [expr, eq] = parser.parse_expression_or_equation();

                if (eq) {
                    Evaluator eval(&ctx, payload);
                    double    lhs = eval.evaluate(eq->lhs());
                    double    rhs = eval.evaluate(eq->rhs());
                    std::cout << "  " << lhs << " = " << rhs;
                    std::cout << (std::abs(lhs - rhs) < 1e-12
                                      ? std::string("  ") + ansi::green +
                                            "(true)" + ansi::reset
                                      : std::string("  ") + ansi::red +
                                            "(false)" + ansi::reset)
                              << "\n";
                    return HistoryStatus::Success;
                } else {
                    Evaluator eval(&ctx, payload);
                    double    val = eval.evaluate(*expr);
                    std::cout << "  = " << val << "\n";
                    return HistoryStatus::Success;
                }
            } catch (const UndefinedVariableError& undef_err) {
                // Suggests similar variable names for undefined identifiers.
                auto match = suggest(undef_err.var_name(), ctx.all_names());
                if (match) {
                    undef_err.with_help(
                        "a variable with a similar name exists: `" + *match +
                        "`");
                }
                std::cout << undef_err.format();
                return HistoryStatus::Error;
            } catch (const MathError& e) {
                std::cout << e.format();
                return HistoryStatus::Error;
            } catch (const std::exception& e) {
                // Defensive: catch-all for unexpected runtime errors.
                std::cout << ansi::red << "  Error: " << ansi::reset << e.what()
                          << "\n";
                return HistoryStatus::Error;
            }
        }

        // Dispatches math commands to the appropriate handler.
        // - Invariant: cmd.type() must be a valid MathCommand::Type.
        // - Returns Unknown for unhandled types (should not occur).
        inline HistoryStatus
        handle_math(const MathCommand& cmd, Context& ctx, Config& config) {
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
            }
            return HistoryStatus::Unknown;
        }

    } // namespace handlers
} // namespace math_solver