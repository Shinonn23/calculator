#include "ui/repl/repl.hpp"
#include "commands/handlers/env_handler.hpp"
#include "commands/registry.hpp"
#include "completions.hpp"
#include "core/diagnostic_sink.hpp"
#include "hints.hpp"
#include "history.hpp"
#include "runner.hpp"
#include "ui/color.hpp"
#include "ui/repl/highlighter.hpp"

#include <replxx.hxx>

#include <iostream>
#include <string>

namespace math_solver {

    namespace {
        // Banner output is intentionally minimal to avoid excessive noise in
        // automated or embedded contexts. Including the environment name is
        // critical for distinguishing concurrent sessions. Any changes here
        // should consider downstream consumers that may parse or depend on
        // startup output.
        void print_banner(const std::string& env_name,
                          const std::string& version) {
            std::cout << ansi::bold << "CMath Solver" << ansi::reset << " v"
                      << version << "  " << ansi::dim << "[" << env_name << "]"
                      << ansi::reset << "\n"
                      << ansi::dim << "Type :help for commands, exit to quit."
                      << ansi::reset << "\n\n";
        }

    } // anonymous namespace

    // Entry point for the interactive REPL.
    //
    // Invariants:
    // - All persistent state (history, environment, config) must be flushed on
    //   normal exit to avoid user data loss.
    // - HandlerRegistry is rebuilt per session to ensure command dispatch
    //   reflects the current environment and configuration.
    // - REPL loop must be robust against EINTR/EAGAIN from the terminal.
    //
    // Subtlety:
    // - History is managed via a custom mechanism; do not rely on replxx's
    //   built-in save/load. This avoids race conditions and ensures
    //   consistency with our persistence model.
    //
    // Performance:
    // - All setup is performed once per session; no per-iteration allocations
    //   or registry rebuilds.
    //
    // Interactions:
    // - On exit, environment and config are always saved, regardless of
    //   session outcome. This is critical for correctness.
    int run_repl(Config& g_config, Context& g_ctx, std::string& g_current_env,
                 const std::string& version) {
        replxx::Replxx    rx;

        const std::string hist_path = setup_history(rx, g_config);

        setup_completions(rx, g_config, g_ctx);
        setup_hints(rx);

        print_banner(g_current_env, version);

        DiagnosticSink  sink;
        HandlerRegistry registry =
            build_handler_registry(g_ctx, g_config, g_current_env, sink);

        registry.set_replxx(rx);

        // Custom history loading is required to maintain consistency with
        // our persistence model. Do not use replxx's built-in history_load.
        registry.load_persisted_history();

        setup_highlighter(rx);

        Runner runner(registry);
        runner.run_interactive(rx, g_current_env);

        // On exit, all persistent state must be saved. Failure to do so
        // risks user data loss or environment corruption.
        handlers::save_current_env(g_config, g_current_env, g_ctx);
        g_config.save();

        // History is persisted in real time; explicit save is unnecessary.
        std::cout << "\n";

        return 0;
    }

} // namespace math_solver