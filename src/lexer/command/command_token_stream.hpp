#pragma once

#include "command_lexer.hpp"
#include "token_stream.hpp"
#include <string>

namespace math_solver {

    // Provides a single-pass token stream abstraction over CommandLexer.
    //
    // Invariant: `current_` always holds the most recently produced token,
    // including Eof. This ensures `peek()` is side-effect free and
    // `advance()` always progresses the stream.
    //
    // - No lookahead or backtracking is supported; this is intentional to
    //   minimize state and avoid subtle bugs in parser/consumer logic.
    // - `raw_input_` is retained verbatim for diagnostics; must match the
    //   original input exactly for correct error reporting.
    // - Construction is intentionally cheap; all lexing is deferred until
    //   tokens are requested. No internal buffering beyond `current_`.
    //
    // Correctness relies on CommandLexer producing a valid token stream
    // (including Eof) and not mutating input state externally.
    class CommandTokenStream : public ITokenStream {
        CommandLexer lexer_;
        CommandToken current_;
        std::string  raw_input_;

        public:
        explicit CommandTokenStream(const std::string& input)
            : lexer_(input), current_(lexer_.next_token()), raw_input_(input) {}
        CommandToken peek() const override { return current_; }

        // Advances the stream by one token.
        // Returns the previous token, updating `current_` to the next.
        // - Correctness relies on the invariant that `current_` is always valid
        // after construction.
        // - No internal buffering beyond single-token lookahead; callers must
        // not retain references to tokens beyond their lifetime.
        CommandToken advance() override {
            CommandToken prev = current_;
            current_          = lexer_.next_token();
            return prev;
        };

        bool is_eof() const override {
            return current_.is(CommandTokenType::Eof);
        }
        const std::string& raw_input() const override { return raw_input_; }

        // Returns the unconsumed suffix of the input, or empty if at Eof.
        // Used for diagnostics; correctness depends on `raw_input_` being
        // unmodified and `tok.start` accurately tracking the current offset.
        std::string        consume_remaining() override {
            const CommandToken& tok = peek();
            if (is_eof())
                return "";
            const std::string& input = raw_input();
            if (tok.start >= input.size())
                return "";
            return input.substr(tok.start);
        }
    };

} // namespace math_solver
