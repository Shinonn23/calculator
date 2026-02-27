#include "var_command_parser.hpp"
#include "ast/command/var_command.hpp"

namespace math_solver {

    // Parses a variable command from the token stream.
    //
    // Invariant: The stream is expected to be positioned at either ":set" or
    // ":unset". The parser distinguishes between set/unset actions based on the
    // first token.
    //
    // If the stream is at EOF after the action, we synthesize a VarCommand with
    // an empty variable name. This is relied upon by downstream consumers to
    // handle incomplete commands gracefully.
    //
    // For ":set", if the next token is a recognized math action ("solve",
    // "expand", "factor"), we treat it as a payload and consume the remainder
    // as its argument. Otherwise, the payload is empty.
    //
    // Note: The parser does not validate the variable name or payload contents
    // here; this is deferred to later passes.
    Result<CommandPtr> VarCommandParser::parse(ITokenStream& stream) {
        bool is_set = (stream.peek().value == ":set");
        stream.advance();
        if (stream.is_eof()) {
            return Result<CommandPtr>::ok(std::make_unique<VarCommand>(
                is_set ? VarCommand::Action::Set : VarCommand::Action::Unset,
                "", stream.raw_input()));
        }
        std::string var_name = stream.peek().value;
        stream.advance();
        auto var_cmd = std::make_unique<VarCommand>(
            is_set ? VarCommand::Action::Set : VarCommand::Action::Unset,
            var_name, stream.raw_input());
        if (is_set) {
            // Only attach a math action payload if the next token is a
            // recognized action. This avoids misinterpreting arbitrary input as
            // a math action.
            if (stream.peek_is(CommandTokenType::Word) &&
                (stream.peek().value == "solve" ||
                 stream.peek().value == "expand" ||
                 stream.peek().value == "factor")) {
                std::string math_action = stream.peek().value;
                stream.advance();
                var_cmd->set_payload(math_action, stream.consume_remaining());
            } else {
                var_cmd->set_payload("", stream.consume_remaining());
            }
        }
        return Result<CommandPtr>::ok(std::move(var_cmd));
    }

} // namespace math_solver
