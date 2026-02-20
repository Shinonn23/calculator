#pragma once

#include "icommand_subparser.hpp"

namespace math_solver {

    // Handles parsing of variable-related commands.
    //
    // Invariant: Assumes the token stream is positioned at the start of a
    // variable command. This parser is invoked only after the main command
    // dispatcher has identified a 'var' command.
    //
    // Note: Any changes to the variable command grammar must be reflected here
    // and in the corresponding semantic analysis pass. Be mindful of
    // interactions with the symbol table and variable scoping rules elsewhere
    // in the pipeline.
    class VarCommandParser : public ICommandSubparser {
        public:
        CommandPtr parse(ITokenStream& stream) override;
    };

} // namespace math_solver
