#pragma once

#include "ast/command/load_command.hpp"
#include "commands/registry.hpp"

#include <replxx.hxx>
#include <string>

namespace math_solver {

    class Runner {
        public:
        explicit Runner(HandlerRegistry& registry) : registry_(registry) {
            registry_.set_runner(*this);
        }

        // Entry point for interactive REPL session.
        // - Assumes `current_env` is a valid environment identifier and may be
        // mutated by commands.
        // - `rx` must be a valid, initialized replxx instance; ownership is not
        // transferred.
        // - Side effects: may mutate global state via registry handlers.
        void run_interactive(replxx::Replxx& rx, std::string& current_env);

        // Executes a script file in batch mode.
        // - `filepath` must refer to a readable file; errors are surfaced via
        // handler return values.
        // - `flags` controls script loading semantics (see LoadCommand::Flags).
        // - Designed for non-interactive use; does not mutate REPL state.
        void run_script(const std::string&        filepath,
                        const LoadCommand::Flags& flags);

        // Executes a single line as a command.
        // - Returns true if the line was handled successfully, false otherwise.
        // - Used by both REPL and script loader; must remain side-effect free
        // except via registry_.
        bool run_line(const std::string& line);

        private:
        // Invariant: registry_ must outlive this Runner instance.
        // All command dispatches are routed through this registry.
        HandlerRegistry& registry_;
    };

} // namespace math_solver