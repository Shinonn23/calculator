#include "system_command_parser.hpp"
#include "ast/command/system_command.hpp"

namespace math_solver {

    // SystemCommandParser::parse is responsible for recognizing and
    // constructing AST nodes for REPL/system-level commands (e.g., exit, help,
    // clear, ls).
    //
    // - Assumes the stream is positioned at the start of a potential system
    // command.
    // - Only exact matches are accepted; no fuzzy or partial matching.
    // - The mapping from string to SystemCommand::Type is intentionally
    // explicit
    //   to avoid accidental acceptance of unintended commands.
    // - Returns nullptr if the input does not correspond to a known system
    // command.
    //
    // Invariant: The returned SystemCommand always reflects the original raw
    // input, which is required for accurate diagnostics and potential command
    // replay.
    //
    // Note: This parser is decoupled from the main expression parser to avoid
    // polluting the core grammar with REPL-specific constructs.
    CommandPtr SystemCommandParser::parse(ITokenStream& stream) {
        std::string cmd = stream.peek().value;
        if (cmd == ":exit" || cmd == ":quit" || cmd == ":q" || cmd == "exit" ||
            cmd == "quit" || cmd == "q") {
            return std::make_unique<SystemCommand>(SystemCommand::Type::Exit,
                                                   stream.raw_input());
        }
        if (cmd == ":help" || cmd == ":h" || cmd == "help" || cmd == "h") {
            return std::make_unique<SystemCommand>(SystemCommand::Type::Help,
                                                   stream.raw_input());
        }
        if (cmd == ":clear" || cmd == ":cls" || cmd == "clear" ||
            cmd == "cls") {
            return std::make_unique<SystemCommand>(SystemCommand::Type::Clear,
                                                   stream.raw_input());
        }
        if (cmd == ":ls") {
            return std::make_unique<SystemCommand>(SystemCommand::Type::Ls,
                                                   stream.raw_input());
        }
        return nullptr;
    }

} // namespace math_solver
