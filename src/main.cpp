#include "common/color.hpp"
#include "common/command.hpp"
#include "common/config.hpp"
#include "common/environment.hpp"
#include "common/error.hpp"
#include "common/replxx.hpp"
#include "common/utils.hpp"
#include "eval/context.hpp"
#include "eval/evaluator.hpp"
#include "parser/cli/cli_parser.hpp"
#include "parser/cli/dispatcher.hpp"
#include "parser/math/math_parser.hpp"
#include <cerrno>
#include <iostream>
#include <replxx.hxx>
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
// REPL dispatch — parse_cli() always returns a CommandAST, dispatch matches it.
// Returns false if the REPL should exit.
// ============================================================================

bool dispatch(const string& raw_input) {
    CommandAST ast = parse_cli(raw_input);
    return dispatch_command(ast, g_ctx, g_config, g_current_env);
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
        for (const auto& [name, expr_str] : env.variables) {
            try {
                Parser parser(expr_str);
                auto   expr = parser.parse();
                g_ctx.set(name, std::move(expr));
            } catch (...) {
                try {
                    double val = std::stod(expr_str);
                    g_ctx.set(name, val);
                } catch (...) {
                }
            }
        }
        g_current_env = auto_env;
    } else {
        if (g_config.env_exists("default")) {
            const auto& env = g_config.get_env("default");
            for (const auto& [name, expr_str] : env.variables) {
                try {
                    Parser parser(expr_str);
                    auto   expr = parser.parse();
                    g_ctx.set(name, std::move(expr));
                } catch (...) {
                    try {
                        double val = std::stod(expr_str);
                        g_ctx.set(name, val);
                    } catch (...) {
                    }
                }
            }
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
