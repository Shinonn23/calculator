#pragma once

#include "ast/command/command.hpp"
#include "lexer/command/token_stream.hpp"

namespace math_solver {

    // Abstract interface for command subparsers.
    //
    // Each implementation is responsible for parsing a specific command variant
    // from the token stream. The interface assumes that the stream is
    // positioned at the start of a command and that ownership of the stream is
    // not transferred.
    //
    // Invariant: Implementations must either consume a valid command or leave
    // the stream in a recoverable state for error handling. Partial consumption
    // is discouraged unless explicitly coordinated with the parser driver.
    //
    // Note: This interface is intended to decouple command parsing logic from
    // the main parser, enabling incremental extension of the command language
    // without impacting unrelated parsing logic.
    class ICommandSubparser {
        public:
        virtual ~ICommandSubparser()                   = default;
        virtual CommandPtr parse(ITokenStream& stream) = 0;
    };

} // namespace math_solver
