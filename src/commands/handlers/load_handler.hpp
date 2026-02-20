#pragma once

#include "ast/command/load_command.hpp"
#include "ui/repl/runner.hpp"

namespace math_solver {
    namespace handlers {
        // Handles the 'load' command by delegating to Runner::run_script.
        //
        // Assumes:
        // - cmd.filepath() yields a valid, accessible file path.
        // - cmd.flags() are validated upstream; no further checking here.
        //
        // This function is intentionally inlined to minimize dispatch overhead,
        // as command handlers may be invoked frequently in REPL scenarios.
        //
        // Note: Any side effects from script execution (e.g., global state
        // changes) are the responsibility of Runner and are not tracked here.
        inline void handle_load(const LoadCommand& cmd, Runner& runner) {
            runner.run_script(cmd.filepath(), cmd.flags());
        }
    } // namespace handlers
} // namespace math_solver