#pragma once

#include "command_token.hpp"
#include <string>

namespace math_solver {

    // Abstract interface for token streams consumed by command parsers.
    //
    // Invariants:
    // - peek() and advance() must always return a valid CommandToken unless
    // is_eof() is true.
    // - raw_input() must return a reference to the original input buffer;
    // lifetime must outlive the stream.
    // - Implementations must ensure that advance() and peek() are consistent
    // (peek() == advance() if not advanced).
    //
    // Performance: Implementations may cache tokens for efficiency, but must
    // not introduce observable side effects.
    //
    // Subtlety: Helper methods like peek_is() are provided to avoid repeated
    // token extraction logic in subparsers.
    class ITokenStream {
        public:
        virtual ~ITokenStream()                      = default;

        virtual CommandToken       peek() const      = 0;
        virtual CommandToken       advance()         = 0;
        virtual bool               is_eof() const    = 0;
        virtual const std::string& raw_input() const = 0;

        // Used by subparsers to branch on token type without advancing.
        bool peek_is(CommandTokenType t) const { return peek().is(t); }

        // Returns the unparsed suffix of the input. Not const: may update
        // internal state.
        virtual std::string consume_remaining() = 0;
    };

} // namespace math_solver
