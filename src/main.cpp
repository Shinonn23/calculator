#include "config/config.hpp"
#include "eval/evaluator.hpp"
#include "parser/math/math_parser.hpp"
#include "runtime/context/context.hpp"
#include "ui/repl/repl.hpp"

#include <iostream>
#include <string>

using namespace math_solver;

// Global state is intentionally static to ensure a single configuration and
// context instance throughout the process lifetime. This avoids subtle bugs
// from context duplication and ensures REPL and CLI modes share the same
// environment.
static Config      g_config;
static Context     g_ctx;
static std::string g_current_env = "default";

// CLI entrypoint. This path is only taken if arguments are provided.
// Note: Context is intentionally stack-local here to avoid polluting global
// state with transient CLI computations. This separation is relied upon by REPL
// startup.
static int         run_cli(int argc, char* argv[]) {
    std::string expr_str;
    for (int i = 1; i < argc; ++i) {
        if (i > 1)
            expr_str += ' ';
        expr_str += argv[i];
    }
    try {
        Context   ctx;
        Parser    parser(expr_str);
        auto      expr = parser.parse();
        Evaluator eval(&ctx, expr_str);
        std::cout << eval.evaluate(*expr) << '\n';
        return 0;
    } catch (const MathError& e) {
        std::cerr << e.format() << '\n';
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
        return 1;
    }
}

// Loads variables from the named environment into the global context at
// startup.
// - Only called once, before REPL is entered.
// - If parsing fails, falls back to interpreting as a double.
// - Silent failure for malformed variables is intentional: environments may
//   contain legacy or user-edited entries.
// - Invariant: After this returns, g_ctx contains all valid variables from env.
static void load_startup_env(const std::string& env_name) {
    if (!g_config.env_exists(env_name))
        return;

    const auto& env = g_config.get_env(env_name);
    for (const auto& [name, expr_str] : env.variables) {
        try {
            Parser parser(expr_str);
            g_ctx.set(name, parser.parse());
        } catch (...) {
            try {
                g_ctx.set(name, std::stod(expr_str));
            } catch (...) {
            }
        }
    }
}

// Entrypoint. Dispatches to CLI or REPL depending on argc.
// - CLI mode is stateless and does not mutate global context.
// - REPL mode loads configuration and environment before entering main loop.
// - The startup environment is determined by config, falling back to "default".
// - Invariant: g_current_env always matches the loaded environment.
int main(int argc, char* argv[]) {
    if (argc > 1)
        return run_cli(argc, argv);

    g_config.load();

    const std::string& auto_env = g_config.settings().auto_load_env;
    const std::string  start_env =
        g_config.env_exists(auto_env) ? auto_env : "default";
    load_startup_env(start_env);
    g_current_env = start_env;

    return run_repl(g_config, g_ctx, g_current_env);
}
