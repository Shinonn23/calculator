#include "config_command_parser.hpp"
#include "ast/command/config_command.hpp"

namespace math_solver {

    CommandPtr ConfigCommandParser::parse(ITokenStream& stream) {
        stream.advance(); // Advance past "config" token; required invariant.

        if (stream.is_eof()) {
            // No subcommand provided; default to List action.
            // This branch is hit for bare "config" invocations.
            return std::make_unique<ConfigCommand>(ConfigCommand::Action::List,
                                                   stream.raw_input());
        }

        std::string sub = stream.peek().value;
        stream.advance();

        ConfigCommand::Action action;
        if (sub == "list")
            action = ConfigCommand::Action::List;
        else if (sub == "get")
            action = ConfigCommand::Action::Get;
        else if (sub == "set")
            action = ConfigCommand::Action::Set;
        else if (sub == "path")
            action = ConfigCommand::Action::Path;
        else if (sub == "reset")
            action = ConfigCommand::Action::Reset;
        else
            action = ConfigCommand::Action::Unknown;

        auto config_cmd =
            std::make_unique<ConfigCommand>(action, stream.raw_input());

        // If the subcommand is unrecognized, preserve the raw subcommand in the
        // key field. This allows downstream diagnostics to report the unknown
        // command verbatim.
        if (action == ConfigCommand::Action::Unknown) {
            config_cmd->set_kv(sub, "");
            return config_cmd;
        }

        std::string key, value;
        if (!stream.is_eof()) {
            key = stream.peek().value;
            stream.advance();
        }
        // Only "set" expects a value; other actions ignore trailing tokens.
        if (action == ConfigCommand::Action::Set && !stream.is_eof())
            value = stream.consume_remaining();

        config_cmd->set_kv(key, value);
        return config_cmd;
    }

} // namespace math_solver
