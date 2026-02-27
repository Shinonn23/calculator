#include "math_command_parser.hpp"
#include "ast/command/math_command.hpp"

namespace math_solver {

    // Maps command string to MathCommand::Type.
    // - Assumes input is always one of the supported commands.
    // - Defaulting to Factor is intentional: parser should not reach here with
    // unknown commands.
    static MathCommand::Type resolve_type(const std::string& cmd) {
        if (cmd == ":solve")
            return MathCommand::Type::Solve;
        if (cmd == ":simplify")
            return MathCommand::Type::Simplify;
        if (cmd == ":expand")
            return MathCommand::Type::Expand;
        return MathCommand::Type::Factor;
    }

    Result<CommandPtr> MathCommandParser::parse(ITokenStream& stream) {
        // Entry point for parsing math commands.
        // - Expects the stream to be positioned at the command token (e.g.,
        // ":solve").
        // - Advances past the command token before processing flags.
        // - Flags are parsed in a single pass; order is not significant.
        // - The "-vars"/"--vars" flag consumes all subsequent Word/QuotedString
        // tokens as variable names.
        //   This is greedy and assumes no ambiguity with other flags.
        // - Any remaining tokens are treated as the command's argument
        // expression.
        MathCommand::Type type = resolve_type(stream.peek().value);
        stream.advance();
        bool                     isolated = false, fraction = false;
        std::vector<std::string> vars;
        while (stream.peek_is(CommandTokenType::Flag)) {
            std::string flag = stream.advance().value;
            if (flag == "-isolated" || flag == "--isolated")
                isolated = true;
            else if (flag == "-fraction" || flag == "--fraction")
                fraction = true;
            else if (flag == "-vars" || flag == "--vars") {
                // Greedily consume all variable names after -vars/--vars.
                // Invariant: no other flags or arguments should appear between
                // -vars and its values.
                while (stream.peek_is(CommandTokenType::Word) ||
                       stream.peek_is(CommandTokenType::QuotedString)) {
                    vars.push_back(stream.advance().value);
                }
            }
        }
        // Construct MathCommand with parsed type and arguments.
        // - set_flags encodes all flag state; no further mutation after
        // construction.
        // - raw_input is preserved for error reporting or diagnostics.
        auto cmd = std::make_unique<MathCommand>(
            type, stream.consume_remaining(), stream.raw_input());
        cmd->set_flags(isolated, fraction, vars);
        return Result<CommandPtr>::ok(std::move(cmd));
    }

} // namespace math_solver
