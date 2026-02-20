#ifndef MATH_SOLVER_COMMAND_PARSER_HPP
#define MATH_SOLVER_COMMAND_PARSER_HPP
#include <string>

#include "ast/command/command.hpp"
#include "lexer/command/command_lexer.hpp"
#include "lexer/command/command_token.hpp"

namespace math_solver {

    class CommandParser {
        private:
        std::string  raw_input_;
        CommandLexer lexer_;
        CommandToken current_;

        // Advances to the next token. Must be called before parsing each
        // construct. Assumes lexer_ is always in a valid state after
        // construction.
        void         advance();

        // Returns the unparsed suffix of the input, starting from the current
        // token. Used for error reporting and diagnostics; does not mutate
        // parser state.
        std::string  get_remaining_payload() const;

        // Sub-parsers for each command category.
        // Each assumes current_ is positioned at the start of the relevant
        // construct. Returns nullptr if the input does not match the expected
        // form.
        CommandPtr   parse_colon_command();
        CommandPtr   parse_bare_word_or_math();

        CommandPtr   parse_system_command();
        CommandPtr   parse_config_command();
        CommandPtr   parse_env_command();
        CommandPtr   parse_var_command();
        CommandPtr   parse_math_command();

        public:
        explicit CommandParser(const std::string& input);

        // Entry point: parses the input into a Command AST.
        // On failure, returns nullptr. Does not throw.
        // Assumes input is a single logical command (not a batch).
        CommandPtr parse();
    };

    // External entry point for command parsing.
    // Thin wrapper for CommandParser; see above for invariants.
    CommandPtr parse_command(const std::string& input);

} // namespace math_solver

#endif // MATH_SOLVER_COMMAND_PARSER_HPP