#include "command_token_stream.hpp"

namespace math_solver {

    // CommandTokenStream maintains a single-token lookahead for command lexing.
    // Invariant: `current_` always holds the next token to be consumed.
    // - This enables LL(1)-style parsing for command grammars.
    // - The raw input is retained for error reporting and diagnostics.
    // - Assumes that `lexer_` is deterministic and stateless between calls to
    // next_token().
    CommandTokenStream::CommandTokenStream(const std::string& input)
        : lexer_(input), current_(lexer_.next_token()), raw_input_(input) {}

    // Advances the stream by one token.
    // Returns the previous token, updating `current_` to the next.
    // - Correctness relies on the invariant that `current_` is always valid
    // after construction.
    // - No internal buffering beyond single-token lookahead; callers must not
    // retain references to tokens beyond their lifetime.
    CommandToken CommandTokenStream::advance() {
        CommandToken prev = current_;
        current_          = lexer_.next_token();
        return prev;
    }

} // namespace math_solver
