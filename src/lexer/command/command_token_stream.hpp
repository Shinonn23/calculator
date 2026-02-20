#pragma once

#include "command_lexer.hpp"
#include "token_stream.hpp"
#include <string>

namespace math_solver {

    // CommandTokenStream provides a token stream abstraction over CommandLexer.
    //
    // Invariant: `current_` always holds the most recently produced token,
    // including Eof. This ensures that `peek()` is side-effect free and
    // `advance()` always progresses the stream.
    //
    // The stream is single-pass; no backtracking or lookahead beyond `peek()`
    // is supported. This is intentional to keep the interface minimal and
    // predictable for downstream consumers (e.g., parser).
    //
    // `raw_input_` is retained for diagnostics and error reporting; it must
    // always match the original input passed at construction.
    //
    // Performance: Construction is cheap; all lexing is deferred until tokens
    // are requested. No internal buffering beyond `current_`.
    class CommandTokenStream : public ITokenStream {
        CommandLexer lexer_;
        CommandToken current_;
        std::string  raw_input_;

        public:
        explicit CommandTokenStream(const std::string& input);

        CommandToken peek() const override { return current_; }
        CommandToken advance() override;
        bool         is_eof() const override {
            return current_.is(CommandTokenType::Eof);
        }
        const std::string& raw_input() const override { return raw_input_; }
    };

} // namespace math_solver
