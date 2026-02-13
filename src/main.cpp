#include "core/ast/equation.hpp"
#include "core/ast/expr.hpp"
#include "core/common/color.hpp"
#include "core/common/command.hpp"
#include "core/common/config.hpp"
#include "core/common/environment.hpp"
#include "core/common/error.hpp"
#include "core/common/replxx.hpp"
#include "core/common/span.hpp"
#include "core/common/suggest.hpp"
#include "core/common/utils.hpp"
#include "core/eval/context.hpp"
#include "core/eval/evaluator.hpp"
#include "core/parser/parser.hpp"
#include "core/solve/simplify.hpp"
#include "core/solve/solver.hpp"
#include <algorithm>
#include <cerrno>
#include <cmath>
#include <iostream>
#include <replxx.hxx>
#include <sstream>
#include <string>
#include <vector>

using namespace std;
using namespace math_solver;

static Config  g_config;
static Context g_ctx;
static string  g_current_env = "default";

static string  build_prompt() {
    string prompt;
    prompt += ansi::bold;
    prompt += "[";
    prompt += g_current_env;
    prompt += "]";
    prompt += ansi::reset;
    prompt += " > ";
    return prompt;
}

// ============================================================================
// REPL dispatch — processes a single input line
// Returns false if the REPL should exit.
// ============================================================================

bool dispatch(const string& input) {
    // Exit commands
    if (input == "exit" || input == "quit" || input == "q")
        return false;

    // :help
    if (input == ":help" || input == ":h") {
        print_help();
        return true;
    }

    // :set
    if (starts_with(input, ":set ")) {
        cmd_set(input.substr(5), g_ctx, g_config);
        return true;
    }

    // :unset
    if (starts_with(input, ":unset ")) {
        cmd_unset(input.substr(7), g_ctx);
        return true;
    }

    // :clear
    if (input == ":clear" || input == ":cls") {
        cmd_clear(g_ctx);
        return true;
    }

    // :vars
    if (input == ":vars") {
        cmd_vars(g_ctx, g_config);
        return true;
    }

    // :config
    if (input == ":config" || starts_with(input, ":config ")) {
        string args = (input.size() > 8) ? trim(input.substr(8)) : "";
        cmd_config(args, g_config);
        return true;
    }

    // :env
    if (input == ":env" || starts_with(input, ":env ")) {
        string args = (input.size() > 5) ? trim(input.substr(5)) : "";
        cmd_env(args, g_current_env, g_config, g_ctx);
        return true;
    }

    // solve
    if (starts_with(input, "solve ")) {
        cmd_solve(input.substr(6), g_ctx, g_config);
        return true;
    }

    // simplify
    if (starts_with(input, "simplify ")) {
        cmd_simplify(input.substr(9), g_config, g_ctx);
        return true;
    }

    // Catch bare commands without colon prefix — suggest colon version
    if (starts_with(input, "set ") || starts_with(input, "unset ") ||
        input == "clear" || input == "cls" || input == "vars" ||
        input == "help" || input == "h" || starts_with(input, "config ") ||
        input == "config" || starts_with(input, "env ") || input == "env") {
        string first_word = split(input)[0];
        cout << ansi::dim << "  Did you mean " << ansi::reset << ansi::bold
             << ":" << first_word << ansi::reset << ansi::dim << "?"
             << ansi::reset << "\n";
        return true;
    }

    // Default: evaluate expression or equation
    cmd_evaluate(input, g_ctx, g_config);

    return true;
}

int main(int argc, char* argv[]) {
    // --- Command-line mode: evaluate expression directly ---
    if (argc > 1) {
        string expr_str;
        for (int i = 1; i < argc; ++i) {
            if (i > 1)
                expr_str += " ";
            expr_str += argv[i];
        }

        Context ctx;
        try {
            Parser    parser(expr_str);
            auto      expr = parser.parse();
            Evaluator eval(&ctx, expr_str);
            cout << eval.evaluate(*expr) << endl;
        } catch (const MathError& e) {
            cerr << e.format() << endl;
            return 1;
        } catch (const exception& e) {
            cerr << "Error: " << e.what() << endl;
            return 1;
        }
        return 0;
    }

    // --- Load config ---
    g_config.load();

    // --- Load auto-load environment ---
    const string& auto_env = g_config.settings().auto_load_env;
    if (g_config.env_exists(auto_env)) {
        const auto& env = g_config.get_env(auto_env);
        for (const auto& [name, value] : env.variables)
            g_ctx.set(name, value);
        g_current_env = auto_env;
    } else {
        if (g_config.env_exists("default")) {
            const auto& env = g_config.get_env("default");
            for (const auto& [name, value] : env.variables)
                g_ctx.set(name, value);
        }
        g_current_env = "default";
    }

    // --- Setup replxx ---
    replxx::Replxx rx;
    string         hist_path = get_history_file_path();
    rx.set_max_history_size(g_config.settings().history_size);
    rx.history_load(hist_path);
    setup_completions(rx, g_config, g_ctx);
    setup_hints(rx);

    // --- Banner ---
    cout << ansi::bold << "Math Solver" << ansi::reset << " v1.0";
    cout << "  " << ansi::dim << "[" << g_current_env << "]" << ansi::reset
         << "\n";
    cout << ansi::dim << "Type :help for commands, exit to quit." << ansi::reset
         << "\n\n";

    // --- REPL loop ---
    while (true) {
        const char* cinput;
        do {
            cinput = rx.input(build_prompt());
        } while (cinput == nullptr && errno == EAGAIN);

        if (cinput == nullptr)
            break; // EOF / Ctrl-D

        string line(cinput);
        line = trim(line);

        if (line.empty())
            continue;

        rx.history_add(line);

        if (!dispatch(line))
            break;
    }

    // --- Cleanup: save env and history ---
    save_current_env_to_config(g_config, g_current_env, g_ctx);
    g_config.save();
    rx.history_save(hist_path);

    cout << "\n";
    return 0;
}
