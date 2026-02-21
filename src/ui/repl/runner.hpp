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
        //
        // - Assumes `current_env` is a valid environment identifier and may be
        //   mutated by commands dispatched through the registry.
        // - `rx` must be a valid, initialized replxx instance; lifetime is
        // managed externally.
        // - Mutates global state only via registry handlers.
        // - REPL state is not guaranteed to be preserved across invocations.
        void run_interactive(replxx::Replxx& rx, std::string& current_env);

        // Batch script execution.
        //
        // - `filepath` must refer to a readable file; errors are surfaced via
        // handler return values.
        // - `flags` controls script loading semantics (see LoadCommand::Flags).
        // - Designed for non-interactive use; does not mutate REPL state.
        // - No side effects outside of registry handler invocations.
        void run_script(const std::string&        filepath,
                        const LoadCommand::Flags& flags);

        // Dispatches a single command line.
        //
        // - Returns true if the command was handled successfully, false
        // otherwise.
        // - Used by both REPL and script loader; must remain side-effect free
        // except via registry_.
        // - Invariant: registry_ must be valid for the lifetime of this Runner.
        bool run_line(const std::string& line);

        private:
        // registry_ must outlive this Runner; all command dispatches are routed
        // through it.
        HandlerRegistry& registry_;

        // Helper for command dispatch; returns true on success.
        // Assumes cmd is a valid, heap-allocated command pointer.
        // May mutate registry state depending on handler implementation.
        bool             dispatch_cmd(CommandPtr& cmd);
    };

} // namespace math_solver