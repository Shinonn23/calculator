#pragma once
#include "icommand_subparser.hpp"

namespace math_solver {

    // MathCommandParser is responsible for parsing math-related commands from the token stream.
    //
    // - Assumes the stream is positioned at the start of a math command.
    // - The parse() implementation must not consume tokens beyond the end of the current command.
    // - Interacts with the global command dispatch; correctness depends on upstream tokenization.
    // - Any changes to command grammar must be reflected here to avoid desync with the parser frontend.
    // - Performance: parse() is on the critical path for command dispatch; avoid unnecessary allocations.
    class MathCommandParser : public ICommandSubparser {
        public:
        CommandPtr parse(ITokenStream& stream) override;
    };

} // namespace math_solver
