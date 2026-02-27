#pragma once

#include "icommand_subparser.hpp"

namespace math_solver {

    // Parses 'load' commands from the token stream.
    // Assumes the stream is positioned at the start of a 'load' command.
    // Returns nullptr on parse failure; caller is responsible for error
    // handling.
    class LoadCommandParser : public ICommandSubparser {
        public:
        Result<CommandPtr> parse(ITokenStream& stream) override;
    };

} // namespace math_solver