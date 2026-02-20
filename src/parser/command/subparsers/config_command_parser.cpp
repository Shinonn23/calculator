#include "config_command_parser.hpp"
#include "ast/command/config_command.hpp"

namespace math_solver {

    CommandPtr ConfigCommandParser::parse(ITokenStream& stream) {
        // The parser expects the stream to be positioned at the 'config' token.
        // Advances past 'config' and dispatches based on the next token, if
        // any.
        //
        // Invariant: If no subcommand is present, defaults to Action::List.
        // Unrecognized subcommands are also treated as List for forward
        // compatibility.
        //
        // For 'set', the remainder of the input is treated as the value,
        // allowing for arbitrary whitespace and embedded tokens. This is relied
        // upon by downstream consumers that expect values to be unparsed.
        //
        // The original raw input is preserved for diagnostics and error
        // reporting.
        stream.advance();
        ConfigCommand::Action action = ConfigCommand::Action::List;
        std::string           key    = "";
        std::string           value  = "";
        if (!stream.is_eof()) {
            std::string sub = stream.peek().value;
            stream.advance();
            // Subcommand dispatch. Only known subcommands are handled
            // explicitly. Any unknown subcommand falls through to List, which
            // is a deliberate choice to avoid hard failures on
            // forward-compatible extensions.
            if (sub == "get")
                action = ConfigCommand::Action::Get;
            else if (sub == "set")
                action = ConfigCommand::Action::Set;
            else if (sub == "path")
                action = ConfigCommand::Action::Path;
            else if (sub == "reset")
                action = ConfigCommand::Action::Reset;
            // Only parse a key if more tokens remain. This is required for all
            // subcommands except List, which ignores the key.
            if (!stream.is_eof()) {
                key = stream.peek().value;
                stream.advance();
            }
            // For 'set', the value may contain spaces and is parsed as the
            // remainder of the input. This avoids tokenization ambiguities and
            // ensures that values are not truncated.
            if (action == ConfigCommand::Action::Set && !stream.is_eof()) {
                value = stream.consume_remaining();
            }
        }
        // The ConfigCommand is always constructed with the original raw input,
        // which is critical for accurate diagnostics and round-trip parsing.
        auto config_cmd =
            std::make_unique<ConfigCommand>(action, stream.raw_input());
        config_cmd->set_kv(key, value);
        return config_cmd;
    }

} // namespace math_solver
