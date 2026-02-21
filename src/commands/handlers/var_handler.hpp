#pragma once

#include "algebra/polynomial/ast_to_poly.hpp"
#include "algebra/polynomial/factor.hpp"
#include "algebra/solver/solver.hpp"
#include "ast/command/history_entry.hpp"
#include "ast/command/var_command.hpp"
#include "config/config.hpp"
#include "core/error.hpp"
#include "eval/evaluator.hpp"
#include "eval/expander.hpp"
#include "parser/math/math_parser.hpp"
#include "runtime/context/context.hpp"
#include "ui/color.hpp"
#include "ui/suggestions.hpp"

#include <iostream>

namespace math_solver {
    namespace handlers {

        // Variable name validation:
        // - Invariant: names must start with alpha/_ and contain only
        // alphanum/_.
        // - This is relied upon by downstream parsing and symbol table logic.
        // - Reserved keywords are disallowed to avoid shadowing and semantic
        // ambiguity.
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
        // - Maintains the invariant that variable names are valid and not
        // reserved.
        // - Updates or inserts variable bindings in the context.
        // - On error, prints diagnostics and leaves context unchanged.
        // - For "solve", clones all context except the target variable to avoid
        //   accidental self-reference during equation solving.
        // - Performance: context cloning is O(n) in the number of variables.
        // - Correctness: relies on parser and solver subsystems for semantic
        // checks.
        inline HistoryStatus handle_set(const VarCommand& cmd, Context& ctx,
                                        Config& config) {
            (void)config;
            using std::cout;
            const std::string& var = cmd.var_name();
            const std::string& raw = cmd.raw_command();

            if (var.empty()) {
                MathError err("missing variable name",
                              find_token_span(raw, ":set"), raw);
                err.with_code("E0401").with_help("Usage: `:set <var> <expr>`");
                cout << err.format();
                return HistoryStatus::Error;
            }

            if (!validate_var_name(var)) {
                if (is_reserved_keyword(var)) {
                    MathError err("`" + var + "` is a reserved keyword",
                                  find_token_span(raw, var), raw);
                    err.with_code("E0402").with_label("reserved word");
                    cout << err.format();
                } else {
                    MathError err("invalid variable name `" + var + "`",
                                  find_token_span(raw, var), raw);
                    err.with_code("E0403")
                        .with_label("invalid identifier")
                        .with_help(
                            "Identifiers must start with a letter or `_` and "
                            "contain only alphanumeric characters.");
                    cout << err.format();
                }
                return HistoryStatus::Error;
            }

            if (!cmd.has_payload()) {
                MathError err("missing expression", find_token_span(raw, var),
                              raw);
                err.with_code("E0404").with_help("Usage: `:set <var> <expr>`");
                cout << err.format();
                return HistoryStatus::Error;
            }

            const std::string& payload     = cmd.payload();
            const std::string& math_action = cmd.math_action();

            // "solve" action:
            // - Avoids self-reference by cloning context without the target
            // variable.
            // - Relies on EquationSolver for semantic and syntactic
            // correctness.
            // - Any MathError is surfaced directly to the user.
            if (math_action == "solve") {
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
                    return HistoryStatus::Success;
                } catch (const MathError& e) {
                    cout << e.format() << "\n";
                    return HistoryStatus::Error;
                }
            }

            // "expand" action:
            // - Converts the expression to a polynomial and stores the expanded
            // form.
            // - Assumes ASTToPolynomial and Polynomial are correct and total.
            if (math_action == "expand") {
                try {
                    Parser     parser(payload);
                    auto       expr = parser.parse();
                    Polynomial poly = ASTToPolynomial(payload).convert(*expr);
                    Parser     sp(poly.to_string());
                    ctx.set(var, sp.parse());
                    cout << "  " << var << " = " << poly.to_string() << "\n";
                    return HistoryStatus::Success;
                } catch (const MathError& e) {
                    cout << e.format() << "\n";
                    return HistoryStatus::Error;
                }
            }

            // "factor" action:
            // - Converts the expression to a polynomial and stores the factored
            // form.
            // - Assumes factor_polynomial is correct and total.
            if (math_action == "factor") {
                try {
                    Parser      parser(payload);
                    auto        expr = parser.parse();
                    auto        poly = ASTToPolynomial(payload).convert(*expr);
                    auto        factored = factor_polynomial(poly);
                    std::string str      = factored.to_string();
                    Parser      sp(str);
                    ctx.set(var, sp.parse());
                    cout << "  " << var << " = " << str << "\n";
                    return HistoryStatus::Success;
                } catch (const MathError& e) {
                    cout << e.format() << "\n";
                    return HistoryStatus::Error;
                }
            }

            // Default assignment:
            // - Parses and stores the expression.
            // - Attempts eager evaluation; if undefined variables are present,
            //   falls back to expansion.
            // - Handles circular dependencies gracefully, emitting a warning.
            try {
                Parser parser(payload);
                ctx.set(var, parser.parse());

                try {
                    Evaluator eval(&ctx, payload);
                    double    val = eval.evaluate(ctx.get_expr(var));
                    cout << "  " << var << " = " << val << "\n";
                    return HistoryStatus::Success;
                } catch (const UndefinedVariableError&) {
                    Expander expander(ctx, payload);
                    try {
                        auto expanded = expander.expand(ctx.get_expr(var));
                        cout << "  " << var << " = " << expanded->to_string()
                             << "\n";
                        return HistoryStatus::Success;
                    } catch (const CircularDependencyError& e) {
                        cout << "  " << var << " = "
                             << ctx.get_expr(var).to_string() << ansi::dim
                             << " (unexpanded)" << ansi::reset << "\n";
                        return HistoryStatus::Warning;
                    }
                } catch (const MathError& e) {
                    cout << e.format() << "\n";
                    return HistoryStatus::Error;
                }
            } catch (const MathError& e) {
                cout << e.format() << "\n";
                return HistoryStatus::Error;
            }
            return HistoryStatus::Error;
        }

        // Removes a variable binding from the context.
        // - If the variable does not exist, suggests similar names.
        // - No-op if the variable is not present.
        // - Suggestion logic is best-effort and may not always be helpful.
        inline HistoryStatus handle_unset(const VarCommand& cmd, Context& ctx) {
            const std::string& var = cmd.var_name();
            const std::string& raw = cmd.raw_command();
            if (var.empty()) {
                MathError err("missing variable name",
                              find_token_span(raw, ":unset"), raw);
                err.with_code("E0401").with_help("Usage: `:unset <var>`");
                std::cout << err.format();
                return HistoryStatus::Error;
            }

            if (ctx.unset(var)) {
                std::cout << "  Removed: " << var << "\n";
                return HistoryStatus::Success;
            } else {
                MathError err("variable `" + var + "` not found",
                              find_token_span(raw, var), raw);
                err.with_code("E0425").with_label("not found in this scope");
                auto match = suggest(var, ctx.all_names());
                if (match) {
                    err.with_help("a variable with a similar name exists: `" +
                                  *match + "`");
                }
                std::cout << err.format();
                return HistoryStatus::Error;
            }
        }

        // Dispatches to the appropriate handler based on the VarCommand action.
        // - Only Set and Unset actions are supported.
        // - If new actions are added, this switch must be updated.
        // - Returns HistoryStatus::Unknown for unhandled actions (should be
        // unreachable).
        inline HistoryStatus handle_var(const VarCommand& cmd, Context& ctx,
                                        Config& config, std::string&) {
            switch (cmd.action()) {
            case VarCommand::Action::Set:
                return handle_set(cmd, ctx, config);
            case VarCommand::Action::Unset:
                return handle_unset(cmd, ctx);
            case VarCommand::Action::Unknown:
                std::string         input   = cmd.raw_command();
                std::string         bad_cmd = input.substr(0, input.find(' '));

                UnknownCommandError e       = UnknownCommandError(
                    bad_cmd, find_token_span(input, bad_cmd), input);

                std::cout << e.format() << "\n";
                return HistoryStatus::Error;
            }
            return HistoryStatus::Unknown;
        }

    } // namespace handlers
} // namespace math_solver