#include "ui/repl/repl.hpp"
#include "commands/handlers/env_handler.hpp"
#include "commands/registry.hpp"
#include "completions.hpp"
#include "hints.hpp"
#include "history.hpp"
#include "parser/command/command_parser.hpp"
#include "ui/color.hpp"
#include "utils/string_utils.hpp"

#include <replxx.hxx>

#include <cerrno>
#include <iostream>
#include <string>

namespace math_solver {

    namespace {

        // Constructs the REPL prompt. The prompt must reflect the current
        // environment to avoid user confusion when switching contexts. Any
        // change to the prompt format may impact completion/hint logic
        // elsewhere.
        std::string build_prompt(const std::string& env_name) {
            return std::string(ansi::bold) + "[" + env_name + "]" +
                   ansi::reset + " > ";
        }

        // Prints the startup banner. The banner is intentionally minimal to
        // avoid excessive output in automated or embedded scenarios. The
        // environment name is included for clarity when running multiple
        // sessions.
        void print_banner(const std::string& env_name) {
            std::cout << ansi::bold << "Math Solver" << ansi::reset << " v1.0"
                      << "  " << ansi::dim << "[" << env_name << "]"
                      << ansi::reset << "\n"
                      << ansi::dim << "Type :help for commands, exit to quit."
                      << ansi::reset << "\n\n";
        }

    } // anonymous namespace

    // Entry point for the interactive REPL loop.
    //
    // - The REPL persists for the lifetime of the process; all stateful
    // resources
    //   (history, environment, registry) are initialized once and reused.
    // - The handler registry is constructed per session to ensure that command
    //   dispatch reflects the current environment and configuration.
    // - The loop is robust against EINTR/EAGAIN from the underlying terminal.
    // - On exit, all persistent state is flushed to disk. Failure to do so may
    //   result in lost history or environment corruption.
    // - Any change to the REPL loop structure must preserve the invariant that
    //   the environment and config are always saved on normal exit.
    int run_repl(Config& g_config, Context& g_ctx, std::string& g_current_env) {
        replxx::Replxx    rx;

        const std::string hist_path = setup_history(rx, g_config);

        setup_completions(rx, g_config, g_ctx);
        setup_hints(rx);

        print_banner(g_current_env);

        HandlerRegistry registry =
            build_handler_registry(g_ctx, g_config, g_current_env);

        while (true) {
            // Defensive: retry input if interrupted by signal (EAGAIN).
            // This avoids spurious REPL termination on transient terminal
            // errors.
            const char* cinput;
            do {
                cinput = rx.input(build_prompt(g_current_env));
            } while (cinput == nullptr && errno == EAGAIN);

            if (cinput == nullptr)
                break; // EOF / Ctrl-D

            std::string line = trim(cinput);
            if (line.empty())
                continue;

            add_history(rx, line);

            CommandPtr cmd = parse_command(line);
            // If dispatch returns false, this signals a request to exit (e.g.,
            // "exit" command).
            if (!registry.dispatch(*cmd))
                break;
        }

        // On exit, persist all state. This is critical for correctness: failure
        // to save the environment or history may result in user data loss.
        handlers::save_current_env(g_config, g_current_env, g_ctx);
        g_config.save();
        save_history(rx, hist_path);
        std::cout << "\n";

        return 0;
    }

} // namespace math_solver