#include "config/config.hpp"
#include "eval/evaluator.hpp"
#include "parser/math/math_parser.hpp"
#include "runtime/context/context.hpp"
#include "ui/repl/repl.hpp"

#include <iostream>
#include <string>

using namespace math_solver;

static const std::string VERSION = "1.1.1";

// CLI entrypoint. This path is only taken if arguments are provided.
// Context is stack-local to prevent transient CLI computations from polluting
// global state. This separation is relied upon by REPL startup, which expects
// ctx to be unmodified by CLI invocations.
static int               run_cli(int argc, char* argv[]) {
    std::string expr_str;
    for (int i = 1; i < argc; ++i) {
        if (i > 1)
            expr_str += ' ';
        expr_str += argv[i];
    }
    try {
        Context ctx;
        Parser  parser(expr_str);
        auto    parse_result = parser.parse();
        if (!parse_result) {
            std::cerr << parse_result.error().format();
            return 1;
        }
        auto      expr = std::move(*parse_result);
        Evaluator eval(&ctx, expr_str);
        std::cout << eval.evaluate(*expr) << '\n';
        return 0;
    } catch (const MathException& e) {
        std::cerr << "Initialization error: " << e.error().message << '\n';
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
        return 1;
    }
}

// Loads variables from the named environment into the global context.
// - Only invoked once before entering REPL.
// - Silent failure for malformed variables is intentional: environments may
//   contain legacy or user-edited entries. This avoids hard failures on
//   partial corruption.
// - Invariant: After return, ctx contains all valid variables from env.
// - If parsing fails, falls back to interpreting as a double. This is a
//   compatibility measure for legacy environments.
static void load_startup_env(const std::string& env_name, Config& config,
                             Context& ctx) {
    if (!config.env_exists(env_name))
        return;

    const auto& env = config.get_env(env_name);
    for (const auto& [name, expr_str] : env.variables) {
        try {
            Parser parser(expr_str);
            auto   parse_result = parser.parse();
            if (!parse_result)
                throw std::runtime_error("parse failed");
            ctx.set(name, std::move(*parse_result));
        } catch (...) {
            try {
                ctx.set(name, std::stod(expr_str));
            } catch (...) {
            }
        }
    }
}

// Entrypoint. Dispatches to CLI or REPL depending on argc.
// - CLI mode is stateless and does not mutate global context.
// - REPL mode loads configuration and environment before entering main loop.
// - The startup environment is determined by config, falling back to "default".
// - Invariant: current_env always matches the loaded environment.
// - Any mutation to ctx or config after this point is observable by REPL
//   and persists for the process lifetime.
int main(int argc, char* argv[]) {
    if (argc > 1)
        return run_cli(argc, argv);

    Config      config;
    Context     ctx;
    std::string current_env = "default";

    config.load();

    const std::string& auto_env = config.settings().auto_load_env;
    const std::string  start_env =
        config.env_exists(auto_env) ? auto_env : "default";
    load_startup_env(start_env, config, ctx);
    current_env = start_env;

    return run_repl(config, ctx, current_env, VERSION);
}
