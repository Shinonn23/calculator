#ifndef COMMAND_H
#define COMMAND_H

#include "../eval/context.hpp"
#include "../eval/evaluator.hpp"
#include "../lexer/token.hpp"
#include "../parser/parser.hpp"
#include "../solve/simplify.hpp"
#include "../solve/solver.hpp"
#include "color.hpp"
#include "config.hpp"
#include "environment.hpp"
#include "suggest.hpp"
#include "utils.hpp"

#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

using namespace std;

namespace math_solver {
    struct CommandFlags {
        vector<string> vars;
        bool           isolated = false;
        bool           fraction = false;
        string         expression;
    };

    inline CommandFlags parse_flags(const string& input) {
        CommandFlags flags;
        size_t       flag_start = input.find(" --");
        if (flag_start == string::npos) {
            flags.expression = input;
            return flags;
        }
        flags.expression         = trim(input.substr(0, flag_start));
        string         flag_part = input.substr(flag_start);
        vector<string> tokens    = split(flag_part);
        for (size_t i = 0; i < tokens.size(); ++i) {
            if (tokens[i] == "--vars") {
                while (i + 1 < tokens.size() &&
                       !starts_with(tokens[i + 1], "--"))
                    flags.vars.push_back(tokens[++i]);
            } else if (tokens[i] == "--isolated") {
                flags.isolated = true;
            } else if (tokens[i] == "--fraction") {
                flags.fraction = true;
            }
        }
        return flags;
    }

    static const vector<string> ALL_COMMANDS = {
        ":set", ":unset", ":clear",   ":vars", ":help", ":config",
        ":env", "solve",  "simplify", "exit",  "quit",
    };

    static const vector<string> CONFIG_SUBCOMMANDS = {
        "list", "get", "set", "path", "reset",
    };

    static const vector<string> ENV_SUBCOMMANDS = {
        "list", "load", "save", "new", "delete",
    };

    inline void cmd_set(const string& args, Context& g_ctx, Config& g_config) {
        vector<string> parts = split(args);
        if (parts.size() < 2) {
            cout << "  Usage: :set <variable> <value>\n";
            return;
        }

        string var_name = parts[0];

        if (var_name.empty() || !(isalpha(var_name[0]) || var_name[0] == '_')) {
            cout << ansi::red << "  Error: " << ansi::reset
                 << "invalid variable name '" << var_name << "'\n";
            return;
        }
        for (char c : var_name) {
            if (!isalnum(c) && c != '_') {
                cout << ansi::red << "  Error: " << ansi::reset
                     << "invalid variable name '" << var_name << "'\n";
                return;
            }
        }
        if (is_reserved_keyword(var_name)) {
            cout << ansi::red << "  Error: " << ansi::reset << "'" << var_name
                 << "' is a reserved keyword\n";
            return;
        }

        string value_str;
        for (size_t i = 1; i < parts.size(); ++i) {
            if (i > 1)
                value_str += " ";
            value_str += parts[i];
        }

        // :set <var> solve <equation> — solve and store result in <var>
        if (starts_with(value_str, "solve ")) {
            string eq_str = value_str.substr(6);
            try {
                Parser  parser(eq_str);
                auto    eq = parser.parse_equation();

                // Build a temp context excluding var_name so that if the
                // target variable appears in the equation it is treated
                // as the unknown to solve for (not substituted).
                Context temp_ctx;
                for (const auto& [name, value] : g_ctx.all()) {
                    if (name != var_name)
                        temp_ctx.set(name, value);
                }

                EquationSolver solver(&temp_ctx, eq_str);
                SolveResult    result = solver.solve(*eq);
                if (result.has_solution) {
                    g_ctx.set(var_name, result.value);
                    cout << "  " << var_name << " = "
                         << fmt(result.value, g_config) << "\n";
                } else {
                    cout << ansi::red << "  Error: " << ansi::reset
                         << "equation has no solution\n";
                }
            } catch (const MathError& e) {
                cout << e.format() << "\n";
                if (dynamic_cast<const MultipleUnknownsError*>(&e)) {
                    cout << ansi::dim
                         << "  Hint: use :set to define variables, or use "
                            "simplify"
                         << ansi::reset << "\n";
                }
            } catch (const exception& e) {
                cout << ansi::red << "  Error: " << ansi::reset << e.what()
                     << "\n";
            }
            return;
        }

        try {
            Parser    parser(value_str);
            auto      expr = parser.parse();
            Evaluator eval(&g_ctx, value_str);
            double    value = eval.evaluate(*expr);
            g_ctx.set(var_name, value);
            cout << "  " << var_name << " = " << fmt(value, g_config) << "\n";
        } catch (const MathError& e) {
            cout << e.format() << "\n";
        } catch (const exception& e) {
            cout << ansi::red << "  Error: " << ansi::reset << e.what() << "\n";
        }
    }

    inline void cmd_unset(const string& args, Context& g_ctx) {
        string var_name = trim(args);
        if (var_name.empty()) {
            cout << "  Usage: :unset <variable>\n";
            return;
        }
        if (g_ctx.unset(var_name)) {
            cout << "  Removed: " << var_name << "\n";
        } else {
            cout << "  Variable '" << var_name << "' not found\n";
            auto match = suggest(var_name, g_ctx.all_names());
            if (match) {
                cout << ansi::dim << "  Did you mean " << ansi::reset
                     << ansi::bold << *match << ansi::reset << ansi::dim << "?"
                     << ansi::reset << "\n";
            }
        }
    }

    inline void cmd_clear(Context& g_ctx) {
        size_t count = g_ctx.size();
        g_ctx.clear();
        cout << "  Cleared " << count << " variable(s)\n";
    }

    inline void cmd_vars(Context& g_ctx, Config& g_config) {
        if (g_ctx.empty()) {
            cout << "  No variables defined\n";
            return;
        }
        // Find max name length for alignment
        size_t max_len = 0;
        for (const auto& [name, _] : g_ctx.all())
            max_len = max(max_len, name.size());

        for (const auto& [name, value] : g_ctx.all()) {
            cout << "  " << name;
            for (size_t i = name.size(); i < max_len; ++i)
                cout << ' ';
            cout << " = " << fmt(value, g_config) << "\n";
        }
    }

    inline void cmd_solve(const string& args, Context& g_ctx, Config& g_config) {
        if (args.empty()) {
            cout << "  Usage: solve <lhs> = <rhs>\n";
            return;
        }
        try {
            Parser          parser(args);
            auto            eq = parser.parse_equation();

            // Find all variables in the equation (without context substitution)
            // If exactly one variable, exclude it from context so that solve
            // always treats it as the unknown — even if it's already defined.
            LinearCollector var_finder(nullptr, args, true);
            LinearForm      lhs_form = var_finder.collect(eq->lhs());
            LinearForm      rhs_form = var_finder.collect(eq->rhs());
            LinearForm      combined = lhs_form - rhs_form;
            auto            eq_vars  = combined.variables();

            Context         temp_ctx;
            const Context*  solve_ctx = &g_ctx;
            if (eq_vars.size() == 1) {
                string target = *eq_vars.begin();
                if (g_ctx.has(target)) {
                    for (const auto& [name, value] : g_ctx.all()) {
                        if (name != target)
                            temp_ctx.set(name, value);
                    }
                    solve_ctx = &temp_ctx;
                }
            }

            EquationSolver solver(solve_ctx, args);
            SolveResult    result = solver.solve(*eq);
            if (result.has_solution) {
                g_ctx.set(result.variable, result.value);
                cout << "  " << result.variable << " = "
                     << fmt(result.value, g_config) << ansi::dim << "  (saved)"
                     << ansi::reset << "\n";
            } else {
                cout << "  " << result.to_string() << "\n";
            }
        } catch (const MathError& e) {
            cout << e.format() << "\n";
            if (dynamic_cast<const MultipleUnknownsError*>(&e)) {
                cout << ansi::dim
                     << "  Hint: use :set to define variables, or use simplify"
                     << ansi::reset << "\n";
            }
        } catch (const exception& e) {
            cout << ansi::red << "  Error: " << ansi::reset << e.what() << "\n";
        }
    }

    inline void cmd_simplify(const string& args, Config& g_config, Context& g_ctx) {
        if (args.empty()) {
            cout << "  Usage: simplify <lhs> = <rhs> [--vars x y] [--isolated] "
                    "[--fraction]\n";
            return;
        }

        CommandFlags flags = parse_flags(args);
        if (flags.expression.empty()) {
            cout << "  Usage: simplify <lhs> = <rhs> [--vars x y] [--isolated] "
                    "[--fraction]\n";
            return;
        }

        // Apply default fraction mode from config
        if (!flags.fraction && g_config.settings().fraction_mode)
            flags.fraction = true;

        try {
            Parser          parser(flags.expression);
            auto            eq = parser.parse_equation();

            SimplifyOptions opts;
            opts.var_order   = flags.vars;
            opts.isolated    = flags.isolated;
            opts.as_fraction = flags.fraction;

            Simplifier     simplifier(&g_ctx, flags.expression);
            SimplifyResult result = simplifier.simplify(*eq, opts);

            for (const auto& warning : result.warnings) {
                cout << ansi::yellow << "  Warning: " << ansi::reset << warning
                     << "\n";
            }

            if (result.is_no_solution()) {
                cout << "  " << result.canonical << "\n";
                cout << "  => " << ansi::red << "no solution" << ansi::reset
                     << "\n";
            } else if (result.is_infinite_solutions()) {
                cout << "  " << result.canonical << "\n";
                cout << "  => " << ansi::green << "infinite solutions"
                     << ansi::reset << "\n";
            } else {
                cout << "  " << result.canonical << "\n";
            }
        } catch (const MathError& e) {
            cout << e.format() << "\n";
        } catch (const exception& e) {
            cout << ansi::red << "  Error: " << ansi::reset << e.what() << "\n";
        }
    }

    inline void cmd_evaluate(const string& input, Context& g_ctx, Config& g_config) {
        try {
            Parser parser(input);
            auto [expr, eq] = parser.parse_expression_or_equation();

            if (eq) {
                Evaluator eval(&g_ctx, input);
                double    lhs_val = eval.evaluate(eq->lhs());
                double    rhs_val = eval.evaluate(eq->rhs());

                cout << "  " << fmt(lhs_val, g_config) << " = "
                     << fmt(rhs_val, g_config);
                if (abs(lhs_val - rhs_val) < 1e-12) {
                    cout << "  " << ansi::green << "(true)" << ansi::reset
                         << "\n";
                } else {
                    cout << "  " << ansi::red << "(false)" << ansi::reset
                         << "\n";
                }
            } else {
                Evaluator eval(&g_ctx, input);
                double    result = eval.evaluate(*expr);
                cout << "  = " << fmt(result, g_config) << "\n";
            }
        } catch (const MathError& e) {
            cout << e.format() << "\n";
        } catch (const exception& e) {
            cout << ansi::red << "  Error: " << ansi::reset << e.what() << "\n";
        }
    }

    inline void cmd_config(const string& args, Config& g_config) {
        vector<string> parts = split(args);

        if (parts.empty()) {
            cout << "  Usage: :config <list|get|set|path|reset>\n";
            return;
        }

        const string& sub = parts[0];

        if (sub == "list") {
            cout << "  " << ansi::bold << "Settings" << ansi::reset << "\n";
            for (const auto& key : Settings::all_keys()) {
                string val = g_config.settings().get(key);
                cout << "    " << ansi::dim << key;
                for (size_t i = key.size(); i < 16; ++i)
                    cout << ' ';
                cout << ansi::reset << val << "\n";
            }
            return;
        }

        if (sub == "get") {
            if (parts.size() < 2) {
                cout << "  Usage: :config get <key>\n";
                cout << "  Keys: ";
                for (const auto& k : Settings::all_keys())
                    cout << k << " ";
                cout << "\n";
                return;
            }
            const string& key = parts[1];
            if (!Settings::is_valid_key(key)) {
                cout << ansi::red << "  Error: " << ansi::reset
                     << "unknown setting '" << key << "'\n";
                maybe_suggest_setting(key);
                return;
            }
            cout << "  " << key << " = " << g_config.settings().get(key)
                 << "\n";
            return;
        }

        if (sub == "set") {
            if (parts.size() < 3) {
                cout << "  Usage: :config set <key> <value>\n";
                return;
            }
            const string& key = parts[1];
            if (!Settings::is_valid_key(key)) {
                cout << ansi::red << "  Error: " << ansi::reset
                     << "unknown setting '" << key << "'\n";
                maybe_suggest_setting(key);
                return;
            }
            // Join remaining parts as value
            string value;
            for (size_t i = 2; i < parts.size(); ++i) {
                if (i > 2)
                    value += " ";
                value += parts[i];
            }
            string err = g_config.settings().set(key, value);
            if (!err.empty()) {
                cout << ansi::red << "  Error: " << ansi::reset << err << "\n";
                return;
            }
            // Validate auto_load_env points to real env
            if (key == "auto_load_env" && !g_config.env_exists(value)) {
                cout << ansi::yellow << "  Warning: " << ansi::reset
                     << "environment '" << value << "' does not exist yet\n";
            }
            g_config.save();
            cout << "  " << key << " = " << g_config.settings().get(key)
                 << "\n";
            return;
        }

        if (sub == "path") {
            cout << "  " << g_config.file_path() << "\n";
            return;
        }

        if (sub == "reset") {
            g_config.reset_settings();
            g_config.save();
            cout << "  Settings reset to defaults\n";
            return;
        }

        // Unknown subcommand — suggest
        cout << ansi::red << "  Error: " << ansi::reset
             << "unknown config command '" << sub << "'\n";
        maybe_suggest_subcommand(sub, CONFIG_SUBCOMMANDS);
    }

    inline void cmd_env(const string& args, string& g_current_env, Config& g_config,
                 Context& g_ctx) {
        vector<string> parts = split(args);

        // :env with no args — show current env
        if (parts.empty()) {
            cout << "  Current environment: " << ansi::bold << g_current_env
                 << ansi::reset << "  (" << g_ctx.size() << " variables)\n";
            return;
        }

        const string& sub = parts[0];

        if (sub == "list") {
            auto envs = g_config.list_envs();
            cout << "  " << ansi::bold << "Environments" << ansi::reset << "\n";
            for (const auto& name : envs) {
                if (name == g_current_env) {
                    cout << "    " << ansi::green << "* " << name << ansi::reset
                         << "  (active)\n";
                } else {
                    cout << "      " << name << "\n";
                }
            }
            return;
        }

        if (sub == "load") {
            if (parts.size() < 2) {
                cout << "  Usage: :env load <name>\n";
                return;
            }
            const string& name = parts[1];
            if (name == g_current_env) {
                cout << "  Already in environment '" << name << "'\n";
                return;
            }
            load_env_into_context(name, g_ctx, g_config, g_current_env);
            if (g_current_env == name) {
                cout << "  Switched to " << ansi::bold << name << ansi::reset
                     << "  (" << g_ctx.size() << " variables)\n";
            }
            return;
        }

        if (sub == "save") {
            string name = (parts.size() >= 2) ? parts[1] : g_current_env;
            // If saving to a different name, create env if needed
            if (!g_config.env_exists(name)) {
                g_config.create_env(name);
            }

            // Check if specific variable names are provided (3rd arg onward)
            if (parts.size() >= 3) {
                // Save only specified variables
                unordered_map<string, double> selected;
                vector<string>                not_found;
                for (size_t i = 2; i < parts.size(); ++i) {
                    const string& var = parts[i];
                    if (g_ctx.has(var)) {
                        selected[var] = g_ctx.all().at(var);
                    } else {
                        not_found.push_back(var);
                    }
                }
                if (!not_found.empty()) {
                    for (const auto& nf : not_found) {
                        cout << ansi::yellow << "  Warning: " << ansi::reset
                             << "variable '" << nf << "' not found, skipped\n";
                        auto match = suggest(nf, g_ctx.all_names());
                        if (match) {
                            cout << ansi::dim << "  Did you mean "
                                 << ansi::reset << ansi::bold << *match
                                 << ansi::reset << ansi::dim << "?"
                                 << ansi::reset << "\n";
                        }
                    }
                }
                if (selected.empty()) {
                    cout << ansi::red << "  Error: " << ansi::reset
                         << "no valid variables to save\n";
                    return;
                }
                // Merge into existing env (update selected, keep others)
                auto& env = g_config.get_env(name);
                for (const auto& [k, v] : selected) {
                    env.variables[k] = v;
                }
                g_config.save();
                cout << "  Saved " << selected.size() << " variable(s) to '"
                     << name << "'\n";
            } else {
                // Save all variables
                g_config.save_env_variables(name, g_ctx.all());
                g_config.save();
                cout << "  Saved " << g_ctx.size() << " variable(s) to '"
                     << name << "'\n";
            }
            return;
        }

        if (sub == "new") {
            if (parts.size() < 2) {
                cout << "  Usage: :env new <name>\n";
                return;
            }
            const string& name = parts[1];
            if (g_config.env_exists(name)) {
                cout << ansi::red << "  Error: " << ansi::reset
                     << "environment '" << name << "' already exists\n";
                return;
            }
            // Save current env first
            save_current_env_to_config(g_config, g_current_env, g_ctx);
            g_config.create_env(name);
            g_ctx.clear();
            g_current_env = name;
            g_config.save();
            cout << "  Created and switched to " << ansi::bold << name
                 << ansi::reset << "\n";
            return;
        }

        if (sub == "delete") {
            if (parts.size() < 2) {
                cout << "  Usage: :env delete <name>\n";
                return;
            }
            const string& name = parts[1];
            if (name == g_current_env) {
                cout << ansi::red << "  Error: " << ansi::reset
                     << "cannot delete the active environment\n";
                cout << ansi::dim
                     << "  Switch to another environment first with :env load"
                     << ansi::reset << "\n";
                return;
            }
            if (!g_config.env_exists(name)) {
                cout << ansi::red << "  Error: " << ansi::reset
                     << "environment '" << name << "' not found\n";
                auto envs  = g_config.list_envs();
                auto match = suggest(name, envs);
                if (match) {
                    cout << ansi::dim << "  Did you mean " << ansi::reset
                         << ansi::bold << *match << ansi::reset << ansi::dim
                         << "?" << ansi::reset << "\n";
                }
                return;
            }
            g_config.delete_env(name);
            g_config.save();
            cout << "  Deleted environment '" << name << "'\n";
            return;
        }

        // Unknown subcommand — suggest
        cout << ansi::red << "  Error: " << ansi::reset
             << "unknown env command '" << sub << "'\n";
        maybe_suggest_subcommand(sub, ENV_SUBCOMMANDS);
    }

    inline void print_help() {
        cout << "\n";
        cout << ansi::bold << "Math Solver " << ansi::reset << "- Commands\n";
        cout << string(40, '-') << "\n\n";

        cout << ansi::bold << "Evaluation" << ansi::reset << "\n";
        cout << "  <expression>              Evaluate (e.g. 2 + 3 * 4)\n";
        cout << "  <lhs> = <rhs>             Check equality\n\n";

        cout << ansi::bold << "Variables" << ansi::reset << "\n";
        cout << "  :set <var> <value>        Set variable\n";
        cout << "  :set <var> solve <eq>     Solve and store in <var>\n";
        cout << "  :unset <var>              Remove variable\n";
        cout << "  :clear                    Clear all variables\n";
        cout << "  :vars                     Show all variables\n\n";

        cout << ansi::bold << "Equations" << ansi::reset << "\n";
        cout << "  solve <lhs> = <rhs>       Solve equation (auto-saves "
                "result)\n";
        cout << "  simplify <lhs> = <rhs>    Simplify to canonical form\n";
        cout << "    --vars x y              Variable order\n";
        cout << "    --isolated              Don't substitute context vars\n";
        cout << "    --fraction              Display as fractions\n\n";

        cout << ansi::bold << "Config" << ansi::reset << "\n";
        cout << "  :config list              Show all settings\n";
        cout << "  :config get <key>         Show setting value\n";
        cout << "  :config set <key> <val>   Update setting\n";
        cout << "  :config path              Show config file path\n";
        cout << "  :config reset             Reset to defaults\n\n";

        cout << ansi::bold << "Environments" << ansi::reset << "\n";
        cout << "  :env                      Show current environment\n";
        cout << "  :env list                 List all environments\n";
        cout << "  :env load <name>          Switch to environment\n";
        cout << "  :env save [name] [vars]   Save variables to env\n";
        cout << "  :env new <name>           Create new environment\n";
        cout << "  :env delete <name>        Delete environment\n\n";

        cout << ansi::bold << "Other" << ansi::reset << "\n";
        cout << "  :help                     Show this help\n";
        cout << "  exit / quit               Exit\n";
        cout << "\n";
    }

} // namespace math_solver

#endif
