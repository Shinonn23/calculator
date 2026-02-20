#include "command_parser.hpp"
#include "ast/command/config_command.hpp"
#include "ast/command/env_command.hpp"
#include "ast/command/math_command.hpp"
#include "ast/command/system_command.hpp"
#include "ast/command/var_command.hpp"
#include "utils/string_utils.hpp"

namespace math_solver {

    CommandParser::CommandParser(const std::string& input)
        : raw_input_(strip_comment(input)),
          lexer_(raw_input_),
          current_(CommandTokenType::Eof, "", 0, 0) {
        advance();
    }

    void CommandParser::advance() {
        current_ = lexer_.next_token();
    }

    std::string CommandParser::get_remaining_payload() const {
        if (current_.is(CommandTokenType::Eof))
            return "";
        // Invariant: current_ points to the first token of the payload.
        // Returns the raw input from the start of the current token to the end,
        // preserving whitespace and comments already stripped.
        return trim(raw_input_.substr(current_.start));
    }

    CommandPtr CommandParser::parse() {
        // Entry point for command parsing. All input is funneled through here.
        // If input is empty, fallback to a MathCommand::Evaluate for
        // consistency.
        if (current_.is(CommandTokenType::Eof)) {
            return std::make_unique<MathCommand>(
                MathCommand::Type::Evaluate, "", raw_input_);
        }

        if (current_.is(CommandTokenType::Command)) {
            return parse_colon_command();
        }

        // If not a colon-prefixed command, treat as either a bare system
        // command or a mathematical expression. This is intentionally
        // permissive to allow for REPL-like user input.
        return parse_bare_word_or_math();
    }

    CommandPtr CommandParser::parse_bare_word_or_math() {
        // Recognizes system commands without a colon prefix.
        // This is a legacy compatibility path for REPL ergonomics.
        if (current_.is(CommandTokenType::Word)) {
            if (current_.value == "exit" || current_.value == "quit" ||
                current_.value == "q") {
                return std::make_unique<SystemCommand>(
                    SystemCommand::Type::Exit, raw_input_);
            }
            if (current_.value == "clear" || current_.value == "cls") {
                return std::make_unique<SystemCommand>(
                    SystemCommand::Type::Clear, raw_input_);
            }
            if (current_.value == "help" || current_.value == "h") {
                return std::make_unique<SystemCommand>(
                    SystemCommand::Type::Help, raw_input_);
            }
        }

        // All other input is treated as a mathematical expression.
        // This ensures that unknown commands are not silently ignored.
        std::string payload = trim(raw_input_);
        return std::make_unique<MathCommand>(
            MathCommand::Type::Evaluate, payload, raw_input_);
    }

    CommandPtr CommandParser::parse_colon_command() {
        std::string cmd = current_.value;

        // System commands with colon prefix. These are canonical forms.
        if (cmd == ":exit" || cmd == ":quit" || cmd == ":q") {
            return std::make_unique<SystemCommand>(SystemCommand::Type::Exit,
                                                   raw_input_);
        }
        if (cmd == ":help" || cmd == ":h") {
            return std::make_unique<SystemCommand>(SystemCommand::Type::Help,
                                                   raw_input_);
        }
        if (cmd == ":clear" || cmd == ":cls") {
            return std::make_unique<SystemCommand>(SystemCommand::Type::Clear,
                                                   raw_input_);
        }
        if (cmd == ":ls") {
            return std::make_unique<SystemCommand>(SystemCommand::Type::Ls,
                                                   raw_input_);
        }

        if (cmd == ":set" || cmd == ":unset" || cmd == ":rm") {
            return parse_var_command();
        }

        if (cmd == ":config" || cmd == ":conf") {
            return parse_config_command();
        }

        if (cmd == ":env") {
            return parse_env_command();
        }

        if (cmd == ":solve" || cmd == ":simplify" || cmd == ":expand" ||
            cmd == ":factor") {
            return parse_math_command();
        }

        // Unknown colon-prefixed commands are treated as MathCommand::Evaluate.
        // This allows for runtime error reporting rather than silent failure.
        return std::make_unique<MathCommand>(
            MathCommand::Type::Evaluate, raw_input_, raw_input_);
    }

    CommandPtr CommandParser::parse_math_command() {
        // Handles math commands with optional flags and variable lists.
        // The flag parsing loop is order-dependent: flags must precede the
        // payload.
        MathCommand::Type type;
        if (current_.value == ":solve")
            type = MathCommand::Type::Solve;
        else if (current_.value == ":simplify")
            type = MathCommand::Type::Simplify;
        else if (current_.value == ":expand")
            type = MathCommand::Type::Expand;
        else
            type = MathCommand::Type::Factor;

        advance();

        bool                     isolated = false;
        bool                     fraction = false;
        std::vector<std::string> vars;

        // Parse flags. Unknown flags are ignored for forward compatibility.
        while (current_.is(CommandTokenType::Flag)) {
            if (current_.value == "-isolated" ||
                current_.value == "--isolated") {
                isolated = true;
                advance();
            } else if (current_.value == "-fraction" ||
                       current_.value == "--fraction") {
                fraction = true;
                advance();
            } else if (current_.value == "-vars" ||
                       current_.value == "--vars") {
                advance();
                while (current_.is(CommandTokenType::Word) ||
                       current_.is(CommandTokenType::QuotedString)) {
                    vars.push_back(current_.value);
                    advance();
                }
            } else {
                advance();
            }
        }

        // Remaining input is the mathematical payload.
        auto math_cmd = std::make_unique<MathCommand>(
            type, get_remaining_payload(), raw_input_);
        math_cmd->set_flags(isolated, fraction, vars);

        return math_cmd;
    }

    CommandPtr CommandParser::parse_var_command() {
        // Handles :set, :unset, :rm commands.
        // Invariant: current_ points to the command token.
        bool is_set = (current_.value == ":set");
        advance();

        if (current_.is(CommandTokenType::Eof)) {
            return std::make_unique<VarCommand>(
                is_set ? VarCommand::Action::Set : VarCommand::Action::Unset,
                "",
                raw_input_);
        }

        std::string var_name = current_.value;
        advance();

        auto var_cmd = std::make_unique<VarCommand>(
            is_set ? VarCommand::Action::Set : VarCommand::Action::Unset,
            var_name,
            raw_input_);

        if (is_set) {
            // If a math action keyword follows, treat as a compound assignment.
            // Otherwise, treat as a plain variable assignment.
            if (current_.is(CommandTokenType::Word) &&
                (current_.value == "solve" || current_.value == "expand" ||
                 current_.value == "factor")) {
                std::string math_action = current_.value;
                advance();
                var_cmd->set_payload(math_action, get_remaining_payload());
            } else {
                var_cmd->set_payload("", get_remaining_payload());
            }
        }

        return var_cmd;
    }

    CommandPtr CommandParser::parse_env_command() {
        // Handles :env commands. Subcommands are optional.
        advance();

        if (current_.is(CommandTokenType::Eof)) {
            return std::make_unique<EnvCommand>(
                EnvCommand::Action::Show, "", raw_input_);
        }

        std::string action_str = current_.value;
        advance();

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

        std::string target_env = "";
        if (!current_.is(CommandTokenType::Eof)) {
            target_env = current_.value;
            advance();
        }

        auto env_cmd =
            std::make_unique<EnvCommand>(action, target_env, raw_input_);

        // For :env save, additional variable names may follow.
        // These are collected and passed to EnvCommand for selective saving.
        if (action == EnvCommand::Action::Save &&
            !current_.is(CommandTokenType::Eof)) {
            std::vector<std::string> vars;
            while (!current_.is(CommandTokenType::Eof)) {
                vars.push_back(current_.value);
                advance();
            }
            env_cmd->set_vars_to_save(vars);
        }

        return env_cmd;
    }

    CommandPtr CommandParser::parse_config_command() {
        // Handles :config and :conf commands.
        // Subcommands are optional; default is Action::List.
        advance();

        ConfigCommand::Action action = ConfigCommand::Action::List;
        std::string           key    = "";
        std::string           value  = "";

        if (!current_.is(CommandTokenType::Eof)) {
            std::string sub = current_.value;
            advance();

            if (sub == "get")
                action = ConfigCommand::Action::Get;
            else if (sub == "set")
                action = ConfigCommand::Action::Set;
            else if (sub == "path")
                action = ConfigCommand::Action::Path;
            else if (sub == "reset")
                action = ConfigCommand::Action::Reset;

            if (!current_.is(CommandTokenType::Eof)) {
                key = current_.value;
                advance();
            }

            // Only :config set expects a value; others ignore trailing input.
            if (action == ConfigCommand::Action::Set &&
                !current_.is(CommandTokenType::Eof)) {
                value = get_remaining_payload();
            }
        }

        auto config_cmd = std::make_unique<ConfigCommand>(action, raw_input_);
        config_cmd->set_kv(key, value);
        return config_cmd;
    }

    CommandPtr parse_command(const std::string& input) {
        // Thin wrapper for external consumers.
        CommandParser parser(input);
        return parser.parse();
    }

} // namespace math_solver