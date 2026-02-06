#ifndef COMMAND_H
#define COMMAND_H
#include "console_ui.h"
#include "eval/evaluator.h"
#include "format_utils.h"
#include "parser/parser.h"
#include "solve/linear_system.h"
#include "solve/simplify.h"
#include "solve/solver.h"
#include "utils.h"

#include <cmath>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace std;

namespace math_solver {
    // Parse flags from command (e.g., --vars, --isolated, --fraction)
    struct CommandFlags {
        vector<string> vars;
        bool           isolated = false;
        bool           fraction = false;
        string         expression;
    };

    CommandFlags parse_flags(const string& input) {
        CommandFlags flags;

        // Find first flag
        size_t       flag_start = input.find("--");
        if (flag_start == string::npos) {
            flags.expression = input;
            return flags;
        }

        flags.expression         = trim(input.substr(0, flag_start));

        // Parse flags
        string         flag_part = input.substr(flag_start);
        vector<string> tokens    = split(flag_part);
        for (size_t i = 0; i < tokens.size(); ++i) {
            if (tokens[i] == "--vars") {
                // Collect variable names until next flag or end
                while (i + 1 < tokens.size() &&
                       !starts_with(tokens[i + 1], "--")) {
                    flags.vars.push_back(tokens[++i]);
                }
            } else if (tokens[i] == "--isolated") {
                flags.isolated = true;
            } else if (tokens[i] == "--fraction") {
                flags.fraction = true;
            }
        }

        return flags;
    }

    void cmd_set(const string& args, Context& ctx, ConsoleUI& ui) {
        vector<string> parts = split(args);
        if (parts.size() < 2) {
            ui.print_usage("set <variable> <value>");
            return;
        }

        string var_name = parts[0];

        // Check if valid identifier
        if (var_name.empty() || !(isalpha(var_name[0]) || var_name[0] == '_')) {
            ui.print_error("invalid variable name '" + var_name + "'");
            return;
        }

        for (char c : var_name) {
            if (!isalnum(c) && c != '_') {
                ui.print_error("invalid variable name '" + var_name + "'");
                return;
            }
        }

        if (is_reserved_keyword(var_name)) {
            ui.print_error("'" + var_name + "' is a reserved keyword");
            return;
        }

        // Parse value (could be an expression)
        string value_str;
        for (size_t i = 1; i < parts.size(); ++i) {
            if (i > 1)
                value_str += " ";
            value_str += parts[i];
        }

        try {
            Parser    parser(value_str);
            auto      expr = parser.parse();
            Evaluator eval(&ctx, value_str);
            double    value = eval.evaluate(*expr);
            ctx.set(var_name, value);

            ui.print_set(var_name, value);

        } catch (const MathError& e) {
            ui.print_error(e);
        } catch (const exception& e) {
            ui.print_error(e.what());
        }
    }

    void cmd_unset(const string& args, Context& ctx, ConsoleUI& ui) {
        string var_name = trim(args);
        if (var_name.empty()) {
            ui.print_usage("unset <variable>");
            return;
        }

        if (ctx.unset(var_name)) {
            ui.print_unset(var_name);
        } else {
            ui.print_unset_not_found(var_name);
        }
    }

    void cmd_clear(Context& ctx, ConsoleUI& ui) {
        size_t count = ctx.size();
        ctx.clear();
        ui.print_clear(count);
    }

    void cmd_vars(const Context& ctx, ConsoleUI& ui) { ui.print_vars(ctx); }

    void cmd_solve(const string& args, Context& ctx, ConsoleUI& ui) {
        if (args.empty()) {
            ui.print_usage("solve <lhs> = <rhs>");
            ui.print_info(
                "       solve  (then enter equations, empty line to solve)");
            return;
        }

        try {
            Parser         parser(args);
            auto           eq = parser.parse_equation();

            EquationSolver solver(&ctx, args);
            SolveResult    result = solver.solve(*eq);

            ui.print_solve(result);

        } catch (const MathError& e) {
            ui.print_error(e);
            // Add hints for common issues
            if (dynamic_cast<const MultipleUnknownsError*>(&e)) {
                ui.print_hint(
                    "use 'solve' alone for multi-variable systems, or "
                    "set to define variables");
            }
        } catch (const exception& e) {
            ui.print_error(e.what());
        }
    }

    // Multi-equation solve mode
    // Returns true if system mode was entered, false if it was a single
    // equation
    bool cmd_solve_system(const string& args, Context& ctx, istream& in,
                          bool interactive, ConsoleUI& ui) {
        CommandFlags flags = parse_flags(args);

        // If there's an expression, it's a single equation (handled by
        // cmd_solve)
        if (!flags.expression.empty()) {
            return false;
        }

        // Multi-equation mode
        if (interactive) {
            ui.print_info("Enter equations (empty line to solve):");
        }

        LinearSystem    system;
        LinearCollector collector(&ctx, "",
                                  false); // Use context for substitution
        int             eq_num = 1;
        string          line;

        while (true) {
            if (interactive) {
                cout << ui.system_prompt(eq_num);
            }
            if (!getline(in, line))
                break;

            line = trim(line);

            // Empty line = done entering equations
            if (line.empty()) {
                break;
            }

            // Allow cancel
            if (line == "cancel") {
                ui.print_info("Cancelled.");
                return true;
            }

            try {
                Parser parser(line);
                auto   eq = parser.parse_equation();

                // Convert to LinearForm
                collector.set_input(line);
                LinearForm lhs        = collector.collect(eq->lhs());
                LinearForm rhs        = collector.collect(eq->rhs());
                LinearForm normalized = lhs - rhs;
                normalized.simplify();

                system.add_equation(normalized);
                ++eq_num;

            } catch (const MathError& e) {
                ui.print_error(e);
                ui.print_info("Try again or type 'cancel' to abort.");
            } catch (const exception& e) {
                ui.print_error(e.what());
                ui.print_info("Try again or type 'cancel' to abort.");
            }
        }

        if (system.empty()) {
            ui.print_info("No equations entered.");
            return true;
        }

        // Sort variables alphabetically unless explicit order given
        if (flags.vars.empty()) {
            system.sort_variables();
        } else {
            system.set_variables(flags.vars);
        }

        // Show system info
        ui.print_solve_system_info(system.num_equations(),
                                   system.num_variables());

        // Warn if under/over-determined
        if (system.num_equations() < system.num_variables()) {
            ui.print_warning(
                "fewer equations than variables (may have infinite solutions)");
        } else if (system.num_equations() > system.num_variables()) {
            ui.print_warning(
                "more equations than variables (may be inconsistent)");
        }

        // Solve
        SystemSolution result = system.solve();

        ui.print_solve_system(result, flags.fraction);

        return true;
    }

    void cmd_simplify(const string& args, Context& ctx, ConsoleUI& ui) {
        if (args.empty()) {
            ui.print_usage("simplify <lhs> = <rhs> [--vars x y] [--isolated] "
                           "[--fraction]");
            return;
        }

        CommandFlags flags = parse_flags(args);

        if (flags.expression.empty()) {
            ui.print_usage("simplify <lhs> = <rhs> [--vars x y] [--isolated] "
                           "[--fraction]");
            return;
        }

        try {
            Parser          parser(flags.expression);
            auto            eq = parser.parse_equation();

            SimplifyOptions opts;
            opts.var_order   = flags.vars;
            opts.isolated    = flags.isolated;
            opts.as_fraction = flags.fraction;

            Simplifier     simplifier(&ctx, flags.expression);
            SimplifyResult result = simplifier.simplify(*eq, opts);

            ui.print_simplify(result);

        } catch (const MathError& e) {
            ui.print_error(e);
        } catch (const exception& e) {
            ui.print_error(e.what());
        }
    }

    void cmd_evaluate(const string& input, Context& ctx, ConsoleUI& ui) {
        try {
            Parser parser(input);
            auto [expr, eq] = parser.parse_expression_or_equation();

            if (eq) {
                // It's an equation - evaluate both sides
                Evaluator eval(&ctx, input);
                double    lhs_val = eval.evaluate(eq->lhs());
                double    rhs_val = eval.evaluate(eq->rhs());
                bool      equal   = abs(lhs_val - rhs_val) < 1e-12;

                ui.print_eval_equation(lhs_val, rhs_val, equal);
            } else {
                // Simple expression
                Evaluator eval(&ctx, input);
                double    result = eval.evaluate(*expr);

                ui.print_eval_expr(result);
            }

        } catch (const MathError& e) {
            ui.print_error(e);
        } catch (const exception& e) {
            ui.print_error(e.what());
        }
    }

    bool process_input_line(const string& raw_input, Context& ctx, istream& in,
                            bool interactive, ConsoleUI& ui) {
        string input = trim(raw_input);

        if (input.empty())
            return true;

        // Exit commands
        if (input == "exit" || input == "q")
            return false;

        // Help (with optional topic)
        if (input == "help") {
            ui.print_help();
            return true;
        }
        if (starts_with(input, "help ")) {
            string topic = trim(input.substr(5));
            ui.print_help(topic);
            return true;
        }

        // Variable commands
        if (starts_with(input, "set ")) {
            cmd_set(input.substr(4), ctx, ui);
            return true;
        }

        if (starts_with(input, "unset ")) {
            cmd_unset(input.substr(6), ctx, ui);
            return true;
        }

        if (input == "clear") {
            cmd_clear(ctx, ui);
            return true;
        }

        if (input == "vars") {
            cmd_vars(ctx, ui);
            return true;
        }

        // Solve command - check for multi-equation mode
        if (input == "solve" || starts_with(input, "solve --")) {
            cmd_solve_system(input.substr(5), ctx, in, interactive, ui);
            return true;
        }

        if (starts_with(input, "solve ")) {
            cmd_solve(input.substr(6), ctx, ui);
            return true;
        }

        // Simplify command
        if (starts_with(input, "simplify ")) {
            cmd_simplify(input.substr(9), ctx, ui);
            return true;
        }

        // Default: evaluate expression or equation
        cmd_evaluate(input, ctx, ui);
        return true;
    }
} // namespace math_solver

#endif