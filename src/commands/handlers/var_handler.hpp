#pragma once

#include "algebra/polynomial/ast_to_poly.hpp"
#include "algebra/polynomial/factor.hpp"
#include "algebra/solver/solver.hpp"
#include "ast/command/var_command.hpp"
#include "config/config.hpp"
#include "eval/evaluator.hpp"
#include "eval/expander.hpp"
#include "parser/math/math_parser.hpp"
#include "runtime/context/context.hpp"
#include "ui/color.hpp"
#include "ui/suggestions.hpp"

#include <iostream>

namespace math_solver {
    namespace handlers {
        // Variable names must be valid identifiers and not reserved keywords.
        // Invariant: names must start with alpha/_ and contain only alphanum/_.
        // This is relied upon by downstream parsing and symbol table logic.
        inline bool validate_var_name(const std::string& name) {
            if (name.empty() || !(std::isalpha(name[0]) || name[0] == '_'))
                return false;
            for (char c : name)
                if (!std::isalnum(c) && c != '_')
                    return false;
            if (is_reserved_keyword(name))
                return false;
            return true;
        }

        // Handles all forms of :set commands.
        // - Handles assignment, expansion, factoring, and equation solving.
        // - Maintains the invariant that variable names are valid and not
        // reserved.
        // - Interacts with the context to update or insert variable bindings.
        // - On error, prints diagnostics and leaves context unchanged.
        // - Performance: For "solve", clones all context except the target
        // variable.
        //   This avoids accidental self-reference during equation solving.
        inline void
        handle_set(const VarCommand& cmd, Context& ctx, Config& config) {
            (void)config;
            using std::cout;
            const std::string& var = cmd.var_name();

            if (var.empty()) {
                cout << "  Usage: :set <var> <expr>\n";
                return;
            }

            if (!validate_var_name(var)) {
                if (is_reserved_keyword(var))
                    cout << ansi::red << "  Error: " << ansi::reset << "'"
                         << var << "' is a reserved keyword\n";
                else
                    cout << ansi::red << "  Error: " << ansi::reset
                         << "invalid variable name '" << var << "'\n";
                return;
            }

            if (!cmd.has_payload()) {
                cout << "  Usage: :set <var> <expr>\n";
                return;
            }

            const std::string& payload     = cmd.payload();
            const std::string& math_action = cmd.math_action();

            if (math_action == "solve") {
                // Solve mode: parse as equation, solve, and bind result.
                // - Uses a temporary context excluding the target variable to
                // avoid
                //   circular dependencies.
                // - Any MathError is surfaced to the user.
                try {
                    Parser  parser(payload);
                    auto    eq = parser.parse_equation();
                    Context temp_ctx;
                    for (const auto& [n, e] : ctx.all())
                        if (n != var)
                            temp_ctx.set(n, *e);

                    EquationSolver solver(&temp_ctx, payload);
                    SolveResult    result = solver.solve(*eq);

                    ctx.set(var, result.value);
                    cout << "  " << var << " = " << result.value << "\n";
                } catch (const MathError& e) {
                    cout << e.format() << "\n";
                }
                return;
            }

            if (math_action == "expand") {
                // Expand mode: parse, convert to polynomial, expand, and bind.
                // - Assumes ASTToPolynomial is lossless for supported
                // expressions.
                // - Any MathError is surfaced to the user.
                try {
                    Parser     parser(payload);
                    auto       expr = parser.parse();
                    Polynomial poly = ASTToPolynomial(payload).convert(*expr);
                    Parser     sp(poly.to_string());
                    ctx.set(var, sp.parse());
                    cout << "  " << var << " = " << poly.to_string() << "\n";
                } catch (const MathError& e) {
                    cout << e.format() << "\n";
                }
                return;
            }

            if (math_action == "factor") {
                // Factor mode: parse, convert to polynomial, factor, and bind.
                // - Relies on factor_polynomial to preserve semantic
                // equivalence.
                // - Any MathError is surfaced to the user.
                try {
                    Parser      parser(payload);
                    auto        expr = parser.parse();
                    auto        poly = ASTToPolynomial(payload).convert(*expr);
                    auto        factored = factor_polynomial(poly);
                    std::string str      = factored.to_string();
                    Parser      sp(str);
                    ctx.set(var, sp.parse());
                    cout << "  " << var << " = " << str << "\n";
                } catch (const MathError& e) {
                    cout << e.format() << "\n";
                }
                return;
            }

            // Default: plain assignment.
            // - Attempts to evaluate numerically if possible, else falls back
            // to symbolic.
            // - Nested try/catch: inner block attempts numeric evaluation,
            // outer block
            //   ensures that even if expansion fails, the symbolic form is
            //   shown.
            // - This fallback chain is important for user experience and
            // robustness.
            try {
                Parser parser(payload);
                ctx.set(var, parser.parse());

                try {
                    Evaluator eval(&ctx, payload);
                    double    val = eval.evaluate(ctx.get_expr(var));
                    cout << "  " << var << " = " << val << "\n";
                } catch (...) {
                    Expander expander(ctx);
                    try {
                        auto expanded = expander.expand(ctx.get_expr(var));
                        cout << "  " << var << " = " << expanded->to_string()
                             << "\n";
                    } catch (...) {
                        cout << "  " << var << " = "
                             << ctx.get_expr(var).to_string() << "\n";
                    }
                }
            } catch (const MathError& e) {
                cout << e.format() << "\n";
            }
        }

        // Removes a variable binding from the context.
        // - If the variable does not exist, suggests similar names.
        // - No-op if the variable is not present.
        inline void handle_unset(const VarCommand& cmd, Context& ctx) {
            const std::string& var = cmd.var_name();
            if (var.empty()) {
                std::cout << "  Usage: :unset <var>\n";
                return;
            }

            if (ctx.unset(var)) {
                std::cout << "  Removed: " << var << "\n";
            } else {
                std::cout << "  Variable '" << var << "' not found\n";
                maybe_suggest(var, ctx.all_names());
            }
        }

        // Dispatches to the appropriate handler based on the VarCommand action.
        // - Invariant: only Set and Unset actions are supported.
        // - Future: If new actions are added, this switch must be updated.
        inline void
        handle_var(const VarCommand& cmd, Context& ctx, Config& config) {
            switch (cmd.action()) {
            case VarCommand::Action::Set:
                handle_set(cmd, ctx, config);
                break;
            case VarCommand::Action::Unset:
                handle_unset(cmd, ctx);
                break;
            }
        }

    } // namespace handlers
} // namespace math_solver