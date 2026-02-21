#pragma once

#include "icommand_subparser.hpp"
#include "lexer/command/token_stream.hpp"

namespace math_solver {

    // Handles parsing of :redo commands, which allow re-execution of previous
    // commands.
    //
    // Invariants:
    // - The selector argument (if present) must resolve to a valid command
    // index or set.
    // - Assumes the command history is consistent and indices are stable across
    // invocations.
    //
    // Subtlety:
    // - Selector parsing must be robust against malformed input (e.g.,
    // overlapping or out-of-bounds ranges).
    // - Interacts with command history subsystem; correctness depends on
    // synchronization with that state.
    //
    // Performance:
    // - Parsing is expected to be fast, as this is on the interactive command
    // path.
    class RedoCommandParser : public ICommandSubparser {
        public:
        CommandPtr parse(ITokenStream& stream) override;
    };

} // namespace math_solver