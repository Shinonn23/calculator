#pragma once

#include <string>

#include "ast/command/command.hpp"

namespace math_solver {

    // Entry point for parsing a single logical command from raw input.
    //
    // - Invariant: `raw_input_` must correspond to exactly one command;
    // batching is not handled here.
    // - Returns nullptr on parse failure; error recovery is deferred to higher
    // layers.
    // - This type is intentionally minimal; all substantive parsing is
    // delegated to subparsers and token stream.
    // - Any changes to parsing entry points must be coordinated with batch
    // parser and REPL input handler to avoid semantic drift.
    // - No external resource ownership; safe for repeated use.
    class CommandParser {
        private:
        std::string raw_input_;

        public:
        explicit CommandParser(const std::string& input);

        // Produces a Command AST or nullptr on failure.
        // Ownership and lifetime semantics: does not retain references to
        // external data.
        CommandPtr parse();
    };

    // Canonical entry point for command parsing.
    //
    // - Must remain in sync with CommandParser invariants.
    // - Used by batch and interactive subsystems; changes here may have
    // cross-cutting effects.
    CommandPtr parse_command(const std::string& input);

} // namespace math_solver
