#ifndef COMMAND_H
#define COMMAND_H
#include "ast/number_array.h"
#include "ast/substitutor.h"
#include "console_ui.h"
#include "eval/evaluator.h"
#include "format_utils.h"
#include "parser/parser.h"
#include "solve/constant_folder.h"
#include "solve/linear_system.h"
#include "solve/nonlinear_system.h"
#include "solve/simplify.h"
#include "solve/solver.h"
#include "utils.h"
#include "value.h"

#include <cmath>
#include <iostream>
#include <set>
#include <sstream>
#include <string>
#include <utility>
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
            Parser parser(value_str);
            auto   expr = parser.parse();

            // Check for circular references before storing
            if (ctx.would_cycle(var_name, *expr)) {
                ui.print_error("circular variable reference detected");
                return;
            }

            // Store the expression symbolically
            string display = expr->to_string();
            ctx.set(var_name, std::move(expr));

            ui.print_set(var_name, display);

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
            ui.print_info("       solve positive/negative/nonneg/integer <eq>");
            return;
        }

        // Check for a SolveFlags keyword at the start
        SolveFlags  flag   = SolveFlags::All;
        std::string eq_str = args;
        {
            // Extract first word
            size_t sp = args.find(' ');
            if (sp != string::npos) {
                string     first_word = args.substr(0, sp);
                SolveFlags f          = parse_solve_flag(first_word);
                if (f != SolveFlags::All) {
                    flag   = f;
                    eq_str = args.substr(sp + 1);
                }
            }
        }

        try {
            Parser                parser(eq_str);
            auto                  eq       = parser.parse_equation();

            // Identify symbolic variables used in the equation
            // and show substitution steps if any were expanded
            auto                  lhs_vars = free_variables(eq->lhs());
            auto                  rhs_vars = free_variables(eq->rhs());
            std::set<std::string> all_vars;
            all_vars.insert(lhs_vars.begin(), lhs_vars.end());
            all_vars.insert(rhs_vars.begin(), rhs_vars.end());

            // Collect substitution info for display
            vector<pair<string, string>> substitutions;
            for (const auto& var : all_vars) {
                const Expr* stored = ctx.get_expr(var);
                if (stored) {
                    // Check if this is a symbolic expression (not just a
                    // number)
                    auto stored_vars = free_variables(*stored);
                    if (!stored_vars.empty()) {
                        substitutions.push_back({var, stored->to_string()});
                    }
                }
            }

            EquationSolver solver(&ctx, args);
            SolveResult    result = solver.solve(*eq);

            // Show substitution steps if symbolic variables were expanded
            if (!substitutions.empty()) {
                // Build the reduced equation string
                // by expanding the equation and simplifying
                try {
                    ExprPtr         expanded_lhs = ctx.expand(eq->lhs());
                    ExprPtr         expanded_rhs = ctx.expand(eq->rhs());

                    // Get the simplified form via LinearCollector
                    LinearCollector collector(nullptr, args, false);
                    LinearForm      lhs_form = collector.collect(*expanded_lhs);
                    LinearForm      rhs_form = collector.collect(*expanded_rhs);
                    LinearForm      normalized = lhs_form - rhs_form;
                    normalized.simplify();

                    // Format reduced equation
                    auto           unknowns = normalized.variables();
                    vector<string> var_order(unknowns.begin(), unknowns.end());
                    sort(var_order.begin(), var_order.end());

                    SimplifyOptions opts;
                    opts.var_order = var_order;
                    Simplifier     simplifier(nullptr, args);
                    Equation       expanded_eq(std::move(expanded_lhs),
                                               std::move(expanded_rhs));
                    SimplifyResult simp =
                        simplifier.simplify(expanded_eq, opts);

                    ui.print_substitution_steps(substitutions, simp.canonical);
                } catch (...) {
                    // If simplification fails, just show substitutions without
                    // reduced form
                    ui.print_substitution_steps(substitutions);
                }
            }

            // Apply post-solve filter (positive, negative, etc.)
            if (flag != SolveFlags::All) {
                size_t removed = apply_solve_flags(result.values, flag);
                if (result.values.empty()) {
                    ui.print_info("no solutions match the requested filter");
                    return;
                }
                if (removed > 0) {
                    ui.print_info(std::to_string(removed) +
                                  " root(s) excluded by filter");
                }
                result.has_solution = !result.values.empty();
            }

            ui.print_solve(result);

            // solve is pure — does NOT store results into context.
            // Use 'let <var> = solve ...' to store results.

        } catch (const MathError& e) {
            ui.print_error(e);
            // Add hints for common issues
            if (dynamic_cast<const MultipleUnknownsError*>(&e)) {
                ui.print_hint(
                    "use 'solve' alone for multi-variable systems, or "
                    "set to define variables");
            } else if (dynamic_cast<const DomainError*>(&e)) {
                ui.print_hint("some roots were excluded because they violate "
                              "the equation's domain (e.g. division by zero)");
            } else if (dynamic_cast<const SolverDivergedError*>(&e)) {
                ui.print_hint("the equation may have no real roots, or roots "
                              "outside the search range [-100, 100]");
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
                double    lhs_val = eval.evaluate_scalar(eq->lhs());
                double    rhs_val = eval.evaluate_scalar(eq->rhs());
                bool      equal   = abs(lhs_val - rhs_val) < 1e-12;

                ui.print_eval_equation(lhs_val, rhs_val, equal);
            } else {
                // Simple expression — try to evaluate numerically
                // If it has free variables after expansion, show symbolic form
                try {
                    Evaluator eval(&ctx, input);
                    Value     result = eval.evaluate(*expr);
                    if (result.is_scalar()) {
                        ui.print_eval_expr(result.as_scalar());
                    } else {
                        ui.print_info(result.to_string());
                    }
                } catch (const UndefinedVariableError&) {
                    // Has unresolved variables — try to show simplified
                    // symbolic form
                    try {
                        ExprPtr         expanded = ctx.expand(*expr);
                        SimplifyOptions opts;
                        Simplifier      simplifier(nullptr, input);
                        SimplifyResult  simp =
                            simplifier.simplify_expr(*expanded, opts);
                        ui.print_info(simp.canonical);
                    } catch (...) {
                        // Re-throw the original error
                        Evaluator eval2(&ctx, input);
                        eval2.evaluate(*expr);
                    }
                }
            }

        } catch (const MathError& e) {
            ui.print_error(e);
        } catch (const exception& e) {
            ui.print_error(e.what());
        }
    }

    // ─── Print command ─────────────────────────────────────────
    void cmd_print(const string& args, Context& ctx, ConsoleUI& ui) {
        if (args.empty()) {
            ui.print_usage("print <expression>");
            return;
        }

        try {
            Parser parser(args);
            auto   expr     = parser.parse();
            string expr_str = expr->to_string();

            // Try evaluation (now supports vectors natively)
            try {
                Evaluator eval(&ctx, args);
                Value     result = eval.evaluate(*expr);
                if (result.is_scalar()) {
                    ui.print_print(expr_str, result.as_scalar());
                } else {
                    // Vector result — format as array
                    ui.print_print_symbolic(expr_str, result.to_string());
                }
            } catch (const MathError& eval_err) {
                // Fallback to expand + constant fold
                try {
                    ExprPtr expanded = ctx.expand(*expr);
                    ExprPtr folded   = fold_constants(*expanded);
                    auto* arr = dynamic_cast<const NumberArray*>(folded.get());
                    if (arr) {
                        ui.print_print_symbolic(expr_str, folded->to_string());
                    } else {
                        auto* num = dynamic_cast<const Number*>(folded.get());
                        if (num) {
                            ui.print_print(expr_str, num->value());
                        } else {
                            string folded_str = folded->to_string();
                            ui.print_print_symbolic(expr_str, folded_str);
                        }
                    }
                } catch (...) {
                    ui.print_error(eval_err);
                }
            }
        } catch (const MathError& e) {
            ui.print_error(e);
        } catch (const exception& e) {
            ui.print_error(e.what());
        }
    }

    // ─── Let command: let <var> = solve <equation> ─────────────
    void cmd_let_solve(const string& args, Context& ctx, ConsoleUI& ui) {
        // Parse: let <var> = solve <equation>
        // args already has "let " stripped, so starts with "<var> = solve ..."

        // Find '='
        size_t eq_pos = args.find('=');
        if (eq_pos == string::npos) {
            ui.print_usage("let <variable> = solve <equation>");
            return;
        }

        string var_name = trim(args.substr(0, eq_pos));
        string rest     = trim(args.substr(eq_pos + 1));

        // Validate variable name
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

        // Check that rest starts with "solve "
        if (!starts_with(rest, "solve ")) {
            ui.print_usage("let <variable> = solve <equation>");
            return;
        }

        string eq_str = trim(rest.substr(6));
        if (eq_str.empty()) {
            ui.print_usage("let <variable> = solve <equation>");
            return;
        }

        try {
            Parser         parser(eq_str);
            auto           eq = parser.parse_equation();

            EquationSolver solver(&ctx, eq_str);
            SolveResult    result = solver.solve(*eq);

            ui.print_solve(result);

            if (result.has_solution) {
                // Warn if storing would overwrite a symbolic dependency
                auto deps = ctx.dependents_of(var_name);
                if (!deps.empty()) {
                    string dep_list;
                    for (const auto& d : deps) {
                        if (!dep_list.empty())
                            dep_list += ", ";
                        dep_list += d;
                    }
                    ui.print_warning("storing '" + var_name +
                                     "' overwrites a symbolic dependency "
                                     "used by: " +
                                     dep_list);
                }

                if (result.values.size() == 1) {
                    ctx.set(var_name, result.values[0]);
                    ui.print_stored(var_name, result.values[0]);
                } else {
                    // Multi-solution: store as NumberArray
                    ctx.set(var_name,
                            std::make_unique<NumberArray>(result.values));
                    ui.print_info("  (stored: " + var_name + " as array with " +
                                  to_string(result.values.size()) +
                                  " solutions)");
                }
            }
        } catch (const MathError& e) {
            ui.print_error(e);
            if (dynamic_cast<const MultipleUnknownsError*>(&e)) {
                ui.print_hint(
                    "use 'solve' alone for multi-variable systems, or "
                    "set to define variables");
            }
        } catch (const exception& e) {
            ui.print_error(e.what());
        }
    }

    // ─── Let destructure: let (x,y) = solve { system } ────────
    void cmd_let_destructure(const string& args, Context& ctx, istream& in,
                             bool interactive, ConsoleUI& ui) {
        // args: "(x, y) = solve { ... }" or "(x, y) = solve {"
        // Find the closing paren
        size_t rparen = args.find(')');
        if (rparen == string::npos) {
            ui.print_usage("let (<var1>, <var2>, ...) = solve { <equations> }");
            return;
        }

        // Extract variable names from (var1, var2, ...)
        string         vars_str = args.substr(1, rparen - 1); // strip parens
        vector<string> var_names;
        {
            vector<string> parts = split(vars_str);
            for (auto& p : parts) {
                // Remove trailing commas
                while (!p.empty() && p.back() == ',')
                    p.pop_back();
                if (!p.empty() && p != ",") {
                    var_names.push_back(p);
                }
            }
        }

        if (var_names.empty()) {
            ui.print_error("no variable names in destructure");
            return;
        }

        // Validate variable names
        for (const auto& vn : var_names) {
            if (vn.empty() || !(isalpha(vn[0]) || vn[0] == '_')) {
                ui.print_error("invalid variable name '" + vn + "'");
                return;
            }
            for (char c : vn) {
                if (!isalnum(c) && c != '_') {
                    ui.print_error("invalid variable name '" + vn + "'");
                    return;
                }
            }
            if (is_reserved_keyword(vn)) {
                ui.print_error("'" + vn + "' is a reserved keyword");
                return;
            }
        }

        // Check for "= solve {"
        string after_parens = trim(args.substr(rparen + 1));
        if (!starts_with(after_parens, "= solve")) {
            ui.print_usage("let (<var1>, <var2>, ...) = solve { <equations> }");
            return;
        }

        string after_solve = trim(after_parens.substr(7)); // skip "= solve"

        // Collect equations — could be inline { eq1 \n eq2 } or multi-line
        vector<string> eq_lines;
        bool           in_block = false;

        if (starts_with(after_solve, "{")) {
            in_block                   = true;
            // Check if there's content after { on the same line
            string content_after_brace = trim(after_solve.substr(1));

            // Check if } is on this line too (inline block)
            size_t close_pos           = content_after_brace.find('}');
            if (close_pos != string::npos) {
                // Inline: let (x,y) = solve { x+y=10, x-y=4 }
                string inline_content =
                    content_after_brace.substr(0, close_pos);
                // Split by comma or newline
                istringstream iss(inline_content);
                string        part;
                while (getline(iss, part, ',')) {
                    part = trim(part);
                    if (!part.empty())
                        eq_lines.push_back(part);
                }
                in_block = false;
            } else {
                // Content after { on same line
                if (!content_after_brace.empty()) {
                    eq_lines.push_back(content_after_brace);
                }
            }
        }

        // Read more lines if we're still in a block
        if (in_block) {
            if (interactive) {
                ui.print_info("Enter equations ('}' to close block):");
            }
            string line;
            int    eq_num = static_cast<int>(eq_lines.size()) + 1;
            while (true) {
                if (interactive) {
                    cout << ui.system_prompt(eq_num);
                }
                if (!getline(in, line))
                    break;
                line               = trim(line);

                // Strip comment from line
                size_t comment_pos = line.find('#');
                if (comment_pos != string::npos) {
                    line = trim(line.substr(0, comment_pos));
                }

                if (line == "}")
                    break;
                // Also handle } at end of line
                size_t brace_pos = line.find('}');
                if (brace_pos != string::npos) {
                    string before = trim(line.substr(0, brace_pos));
                    if (!before.empty())
                        eq_lines.push_back(before);
                    break;
                }
                if (!line.empty()) {
                    eq_lines.push_back(line);
                    ++eq_num;
                }
            }
        }

        if (eq_lines.empty()) {
            ui.print_error("no equations provided for system solve");
            return;
        }

        // Build and solve the system
        try {
            // First, parse all equations
            std::vector<std::unique_ptr<Equation>> parsed_eqs;
            for (const auto& eq_str : eq_lines) {
                Parser parser(eq_str);
                parsed_eqs.push_back(parser.parse_equation());
            }

            // Try linear system first
            bool solved_linear = false;
            try {
                LinearSystem    system;
                LinearCollector collector(&ctx, "", false);

                for (size_t i = 0; i < parsed_eqs.size(); ++i) {
                    collector.set_input(eq_lines[i]);
                    LinearForm lhs = collector.collect(parsed_eqs[i]->lhs());
                    LinearForm rhs = collector.collect(parsed_eqs[i]->rhs());
                    LinearForm normalized = lhs - rhs;
                    normalized.simplify();
                    system.add_equation(normalized);
                }

                if (!system.empty()) {
                    system.sort_variables();

                    ui.print_solve_system_info(system.num_equations(),
                                               system.num_variables());

                    SystemSolution result = system.solve();

                    if (result.type == SolutionType::Unique) {
                        if (var_names.size() != result.variables.size()) {
                            ui.print_solve_system(result, false);
                            ui.print_error(
                                "destructure count mismatch: declared " +
                                to_string(var_names.size()) +
                                " variables but system has " +
                                to_string(result.variables.size()));
                            return;
                        }

                        ui.print_solve_system(result, false);

                        for (size_t i = 0; i < var_names.size(); ++i) {
                            ctx.set(var_names[i], result.values[i]);
                            ui.print_stored(var_names[i], result.values[i]);
                        }
                        solved_linear = true;
                    } else {
                        ui.print_solve_system(result, false);
                        solved_linear = true; // Even if no solution, handled
                    }
                }
            } catch (const NonLinearError&) {
                // Fall through to nonlinear solver
            }

            if (!solved_linear) {
                // Nonlinear system solver
                // Expand equations through context
                std::vector<std::unique_ptr<Equation>> expanded_eqs;
                std::set<std::string>                  all_free_vars;

                for (const auto& eq : parsed_eqs) {
                    ExprPtr exp_lhs = ctx.expand(eq->lhs());
                    ExprPtr exp_rhs = ctx.expand(eq->rhs());

                    auto    lv      = free_variables(*exp_lhs);
                    auto    rv      = free_variables(*exp_rhs);
                    all_free_vars.insert(lv.begin(), lv.end());
                    all_free_vars.insert(rv.begin(), rv.end());

                    expanded_eqs.push_back(std::make_unique<Equation>(
                        std::move(exp_lhs), std::move(exp_rhs)));
                }

                // Sort variable names alphabetically
                std::vector<std::string> sys_vars(all_free_vars.begin(),
                                                  all_free_vars.end());
                std::sort(sys_vars.begin(), sys_vars.end());

                ui.print_solve_system_info(expanded_eqs.size(),
                                           sys_vars.size());

                NonlinearSystemSolver nl_solver(&ctx, "");
                NonlinearSolution     nl_result =
                    nl_solver.solve(expanded_eqs, sys_vars);

                if (nl_result.type != SolutionType::Unique) {
                    ui.print_error("no solution found for nonlinear system");
                    return;
                }

                // Map system variables to user-declared names
                if (var_names.size() != sys_vars.size()) {
                    // Print solution first
                    string sol_msg = "Solution:";
                    for (size_t i = 0; i < sys_vars.size(); ++i) {
                        sol_msg += "\n  " + sys_vars[i] + " = " +
                                   format_double(nl_result.values[i]);
                    }
                    ui.print_info(sol_msg);
                    ui.print_error("destructure count mismatch: declared " +
                                   to_string(var_names.size()) +
                                   " variables but system has " +
                                   to_string(sys_vars.size()));
                    return;
                }

                // Print and store — match by sorted order
                string sol_msg = "Solution:";
                for (size_t i = 0; i < sys_vars.size(); ++i) {
                    sol_msg += "\n  " + sys_vars[i] + " = " +
                               format_double(nl_result.values[i]);
                }
                ui.print_info(sol_msg);

                // Show all solutions if multiple
                if (nl_result.all_solutions.size() > 1) {
                    ui.print_info("  (" +
                                  to_string(nl_result.all_solutions.size()) +
                                  " solution sets found, using first)");
                }

                for (size_t i = 0; i < var_names.size(); ++i) {
                    ctx.set(var_names[i], nl_result.values[i]);
                    ui.print_stored(var_names[i], nl_result.values[i]);
                }
            }

        } catch (const MathError& e) {
            ui.print_error(e);
        } catch (const exception& e) {
            ui.print_error(e.what());
        }
    }

    bool process_input_line(const string& raw_input, Context& ctx, istream& in,
                            bool interactive, ConsoleUI& ui) {
        string input       = trim(raw_input);

        // Strip comments: everything after # (outside of command context)
        size_t comment_pos = input.find('#');
        if (comment_pos != string::npos) {
            input = trim(input.substr(0, comment_pos));
        }

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

        // Print command
        if (starts_with(input, "print ")) {
            cmd_print(input.substr(6), ctx, ui);
            return true;
        }

        // Let command: let <var> = solve <eq> or let (vars) = solve { ... }
        if (starts_with(input, "let ")) {
            string let_args = trim(input.substr(4));
            if (!let_args.empty() && let_args[0] == '(') {
                cmd_let_destructure(let_args, ctx, in, interactive, ui);
            } else {
                cmd_let_solve(let_args, ctx, ui);
            }
            return true;
        }

        // Simplify command
        if (starts_with(input, "simplify ")) {
            cmd_simplify(input.substr(9), ctx, ui);
            return true;
        }

        // Mode command: set evaluation mode
        if (input == "mode" || starts_with(input, "mode ")) {
            string mode_arg = (input == "mode") ? "" : trim(input.substr(5));
            if (mode_arg.empty()) {
                // Show current mode
                string current;
                switch (ctx.eval_mode()) {
                case EvalMode::Numeric:
                    current = "numeric";
                    break;
                case EvalMode::Symbolic:
                    current = "symbolic";
                    break;
                case EvalMode::Vector:
                    current = "vector";
                    break;
                }
                ui.print_info("current mode: " + current);
                ui.print_info("available: numeric, symbolic, vector");
            } else if (mode_arg == "numeric") {
                ctx.set_eval_mode(EvalMode::Numeric);
                ui.print_info("mode set to numeric");
            } else if (mode_arg == "symbolic") {
                ctx.set_eval_mode(EvalMode::Symbolic);
                ui.print_info("mode set to symbolic");
            } else if (mode_arg == "vector") {
                ctx.set_eval_mode(EvalMode::Vector);
                ui.print_info("mode set to vector");
            } else {
                ui.print_error("unknown mode: " + mode_arg);
                ui.print_info("available: numeric, symbolic, vector");
            }
            return true;
        }

        // Default: evaluate expression or equation
        cmd_evaluate(input, ctx, ui);
        return true;
    }
} // namespace math_solver

#endif