#pragma once

#include "icommand_subparser.hpp"

namespace math_solver {

    // SystemCommandParser is responsible for parsing system-level commands
    // (e.g., shell invocations or environment manipulations) from the token
    // stream.
    //
    // - Assumes the stream is positioned at the start of a system command.
    // - Must not consume tokens belonging to subsequent commands.
    // - Interacts with the global command dispatch; changes here may impact
    //   command resolution and error reporting downstream.
    // - Performance: parse() is expected to be called infrequently, so
    //   correctness and robustness are prioritized over micro-optimizations.
    class SystemCommandParser : public ICommandSubparser {
        public:
        Result<CommandPtr> parse(ITokenStream& stream) override;
    };

} // namespace math_solver
