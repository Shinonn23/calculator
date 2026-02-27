#pragma once

#include "ast/command/env_command.hpp"
#include "icommand_subparser.hpp"

namespace math_solver {

    // EnvCommandParser is responsible for parsing environment-related commands.
    //
    // - Assumes the token stream is positioned at the start of an env command.
    // - The parse_move_copy and parse_save methods are invoked by the main
    // parse
    //   entry point based on the detected action.
    // - Correctness relies on the token stream not being mutated externally
    // during parsing.
    // - Any changes to EnvCommand::Action or the command AST must be reflected
    // here to
    //   avoid silent misparsing.
    //
    // Note: This parser is performance-sensitive as it is invoked in the hot
    // path of command dispatch. Avoid introducing unnecessary allocations or
    // virtual dispatch.
    class EnvCommandParser : public ICommandSubparser {
        public:
        Result<CommandPtr> parse(ITokenStream& stream) override;
        Result<CommandPtr> parse_move_copy(ITokenStream&      stream,
                                           EnvCommand::Action action);
        Result<CommandPtr> parse_save(ITokenStream& stream);
    };

} // namespace math_solver
