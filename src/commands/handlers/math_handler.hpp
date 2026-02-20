#pragma once

#include "algebra/linear/simplify.hpp"
#include "algebra/polynomial/ast_to_poly.hpp"
#include "algebra/polynomial/factor.hpp"
#include "algebra/solver/solver.hpp"
#include "ast/command/math_command.hpp"
#include "config/config.hpp"
#include "eval/evaluator.hpp"
#include "eval/expander.hpp"
#include "parser/math/math_parser.hpp"
#include "runtime/context/context.hpp"
#include "ui/color.hpp"

#include <iostream>

namespace math_solver {
    namespace handlers {
        inline void
        do_solve(const std::string& payload, Context& ctx, Config& /*config*/) {
            if (payload.empty()) {
                std::cout << "  Usage: :solve <lhs> = <rhs>\n";
                return;
            }
            try {
                Parser                parser(payload);
                auto                  eq = parser.parse_equation();

                // Attempt to infer unknowns using context-aware substitution.
                // If context is incomplete or substitution fails, fall back to
                // a context-free collection. This is necessary to avoid
                // misidentifying knowns as unknowns when context is partial.
                std::set<std::string> unknowns;
                try {
                    LinearCollector lc(&ctx, payload, false);
                    unknowns = (lc.collect(eq->lhs()) - lc.collect(eq->rhs()))
                                   .variables();
                } catch (...) {
                    LinearCollector lc(nullptr, payload, true);
                    unknowns = (lc.collect(eq->lhs()) - lc.collect(eq->rhs()))
                                   .variables();
                }

                // If exactly one unknown, exclude it from the context to avoid
                // accidental shadowing by a previously set value. This ensures
                // the solver does not treat the target as a constant.
                const Context* solve_ctx = &ctx;
                Context        temp_ctx;
                if (unknowns.size() == 1) {
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

                // Always update the context with the solution, even if the
                // variable was previously set. This is consistent with REPL
                // semantics and avoids stale bindings.
                ctx.set(result.variable, result.value);
                std::cout << "  " << result.variable << " = " << result.value
                          << ansi::dim << " (saved)" << ansi::reset << "\n";

            } catch (const MathError& e) {
                std::cout << e.format() << "\n";
                // NonLinearError is surfaced here to hint at missing variable
                // definitions, which is a common user error.
                if (dynamic_cast<const NonLinearError*>(&e))
                    std::cout << ansi::dim
                              << "  Hint: use :set to define variables\n"
                              << ansi::reset;
            }
        }

        inline void do_simplify(const std::string& payload,
                                const MathCommand& cmd,
                                Context&           ctx,
                                Config&            config) {
            if (payload.empty()) {
                std::cout << "  Usage: :simplify <lhs> = <rhs> [-vars x y] "
                             "[-isolated] [-fraction]\n";
                return;
            }
            try {
                Parser          parser(payload);
                auto            eq = parser.parse_equation();

                // Option propagation: prefer explicit command-line flags, but
                // fall back to global config for fraction mode. This ensures
                // deterministic behavior across invocations.
                SimplifyOptions opts;
                opts.var_order = cmd.specific_vars();
                opts.isolated  = cmd.isolated();
                opts.as_fraction =
                    cmd.as_fraction() || config.settings().fraction_mode;

                Simplifier     simplifier(&ctx, payload);
                SimplifyResult result = simplifier.simplify(*eq, opts);

                // Warnings are surfaced directly; no attempt is made to
                // suppress or deduplicate, as downstream consumers may rely
                // on full diagnostic output.
                for (const auto& w : result.warnings)
                    std::cout << ansi::yellow << "  Warning: " << ansi::reset
                              << w << "\n";

                std::cout << "  " << result.canonical;
                if (result.is_no_solution())
                    std::cout << "\n  => " << ansi::red << "no solution"
                              << ansi::reset;
                else if (result.is_infinite_solutions())
                    std::cout << "\n  => " << ansi::green
                              << "infinite solutions" << ansi::reset;
                std::cout << "\n";

            } catch (const MathError& e) {
                std::cout << e.format() << "\n";
            }
        }

        inline void do_expand(const std::string& payload) {
            if (payload.empty()) {
                std::cout << "  Usage: :expand <expr>\n";
                return;
            }
            try {
                Parser     parser(payload);
                auto       expr = parser.parse();
                Polynomial poly = ASTToPolynomial(payload).convert(*expr);
                std::cout << "  " << poly.to_string() << "\n";
            } catch (const MathError& e) {
                std::cout << e.format() << "\n";
            }
        }

        inline void do_factor(const std::string& payload) {
            if (payload.empty()) {
                std::cout << "  Usage: :factor <expr>\n";
                return;
            }
            try {
                Parser parser(payload);
                auto   expr     = parser.parse();
                auto   poly     = ASTToPolynomial(payload).convert(*expr);
                auto   factored = factor_polynomial(poly);
                std::cout << "  " << factored.to_string() << "\n";
            } catch (const MathError& e) {
                std::cout << e.format() << "\n";
            }
        }

        inline void do_evaluate(const std::string& payload,
                                Context&           ctx,
                                Config& /*config*/) {
            if (payload.empty())
                return;
            try {
                Parser parser(payload);
                auto [expr, eq] = parser.parse_expression_or_equation();

                if (eq) {
                    // Evaluate both sides numerically and compare for equality
                    // within a tight epsilon. This is not robust for all
                    // floating-point scenarios, but suffices for REPL usage.
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
                } else {
                    // Attempt numeric evaluation first; if variables are
                    // undefined, fall back to symbolic expansion. This
                    // two-stage approach avoids unnecessary symbolic work.
                    try {
                        Evaluator eval(&ctx, payload);
                        std::cout << "  = " << eval.evaluate(*expr) << "\n";
                    } catch (const UndefinedVariableError&) {
                        Expander expander(ctx);
                        try {
                            std::cout << "  = "
                                      << expander.expand(*expr)->to_string()
                                      << "\n";
                        } catch (const CircularDependencyError& e) {
                            std::cout << ansi::red << "  Error: " << ansi::reset
                                      << e.what() << "\n";
                        }
                    }
                }
            } catch (const MathError& e) {
                std::cout << e.format() << "\n";
            } catch (const std::exception& e) {
                std::cout << ansi::red << "  Error: " << ansi::reset << e.what()
                          << "\n";
            }
        }
        inline void
        handle_math(const MathCommand& cmd, Context& ctx, Config& config) {
            const std::string& payload = cmd.payload();
            // Dispatch based on command type. This is intentionally not
            // extensible at runtime; new commands require explicit addition.
            switch (cmd.type()) {
            case MathCommand::Type::Solve:
                do_solve(payload, ctx, config);
                break;
            case MathCommand::Type::Simplify:
                do_simplify(payload, cmd, ctx, config);
                break;
            case MathCommand::Type::Expand:
                do_expand(payload);
                break;
            case MathCommand::Type::Factor:
                do_factor(payload);
                break;
            case MathCommand::Type::Evaluate:
                do_evaluate(payload, ctx, config);
                break;
            }
        }

    } // namespace handlers
} // namespace math_solver