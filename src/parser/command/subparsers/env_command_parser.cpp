#include "env_command_parser.hpp"
#include "ast/command/env_command.hpp"

namespace math_solver {

    CommandPtr EnvCommandParser::parse(ITokenStream& stream) {
        stream
            .advance(); // always consume the ":env" prefix; required invariant

        // If no further tokens, default to Show action. This is the fallback
        // for bare ":env".
        if (stream.is_eof()) {
            return std::make_unique<EnvCommand>(
                EnvCommand::Action::Show, "", stream.raw_input());
        }

        std::string action_str = stream.peek().value;
        stream.advance();

        // Action dispatch: string-to-enum mapping.
        // Note: aliases (e.g., "ls" for "list") are handled here for CLI
        // ergonomics.
        EnvCommand::Action action = EnvCommand::Action::Show;
        if (action_str == "list" || action_str == "ls")
            action = EnvCommand::Action::List;
        else if (action_str == "load")
            action = EnvCommand::Action::Load;
        else if (action_str == "save")
            action = EnvCommand::Action::Save;
        else if (action_str == "new")
            action = EnvCommand::Action::New;
        else if (action_str == "delete")
            action = EnvCommand::Action::Delete;
        else if (action_str == "move" || action_str == "mv")
            action = EnvCommand::Action::Move;
        else if (action_str == "copy" || action_str == "cp")
            action = EnvCommand::Action::Copy;

        // Move/Copy require special handling due to dual modes (vars/env).
        if (action == EnvCommand::Action::Move ||
            action == EnvCommand::Action::Copy) {
            return parse_move_copy(stream, action);
        }

        // Save supports optional variable selection; handled separately.
        if (action == EnvCommand::Action::Save) {
            return parse_save(stream);
        }

        // For all other actions, at most one argument (target_env) is expected.
        std::string target_env;
        if (!stream.is_eof()) {
            target_env = stream.peek().value;
            stream.advance();
        }

        return std::make_unique<EnvCommand>(
            action, target_env, stream.raw_input());
    }

    CommandPtr EnvCommandParser::parse_move_copy(ITokenStream&      stream,
                                                 EnvCommand::Action action) {
        EnvCommand::Flags flags;
        std::string       source_env;
        std::string       target_env;

        // --vars mode: explicit variable selection, requires "--to" for
        // destination. Invariant: if --vars is present, must be followed by at
        // least one var and "--to".
        if (!stream.is_eof() && stream.peek().value == "--vars") {
            stream.advance();
            flags.vars_mode = true;

            std::vector<std::string> vars;
            while (!stream.is_eof() && stream.peek().value != "--to") {
                vars.push_back(stream.peek().value);
                stream.advance();
            }

            // "--to" is mandatory in vars_mode; if missing, to_env remains
            // empty.
            if (!stream.is_eof() && stream.peek().value == "--to") {
                stream.advance();
                if (!stream.is_eof()) {
                    flags.to_env = stream.peek().value;
                    stream.advance();
                }
            }

            // Note: set_flags and set_vars_to_save must be called to preserve
            // mode and selection.
            auto cmd = std::make_unique<EnvCommand>(
                action, flags.to_env, stream.raw_input());
            cmd->set_flags(flags);
            cmd->set_vars_to_save(vars);
            return cmd;
        }

        // env mode: expects two positional arguments (source_env, target_env).
        // If either is missing, empty string is passed; downstream must handle.
        if (!stream.is_eof()) {
            source_env = stream.peek().value;
            stream.advance();
        }
        if (!stream.is_eof()) {
            target_env = stream.peek().value;
            stream.advance();
        }

        auto cmd = std::make_unique<EnvCommand>(
            action, target_env, stream.raw_input());
        cmd->set_source_env(source_env);
        cmd->set_flags(flags); // vars_mode = false by default
        return cmd;
    }

    CommandPtr EnvCommandParser::parse_save(ITokenStream& stream) {
        std::string              target_env;
        std::vector<std::string> vars;

        // target_env is optional; must not start with "--".
        if (!stream.is_eof() && stream.peek().value.rfind("--", 0) != 0) {
            target_env = stream.peek().value;
            stream.advance();
        }

        // "--vars" enables selective variable save; all remaining tokens are
        // variable names.
        if (!stream.is_eof() && stream.peek().value == "--vars") {
            stream.advance();
            while (!stream.is_eof()) {
                vars.push_back(stream.peek().value);
                stream.advance();
            }
        }

        auto cmd = std::make_unique<EnvCommand>(
            EnvCommand::Action::Save, target_env, stream.raw_input());
        if (!vars.empty())
            cmd->set_vars_to_save(vars);
        return cmd;
    }

} // namespace math_solver