#ifndef MATH_SOLVER_CMD_LEXER_HPP
#define MATH_SOLVER_CMD_LEXER_HPP

#include "command_token.hpp"
#include <cctype>
#include <string>

namespace math_solver {

    class CommandLexer {
        private:
        std::string input_;
        size_t      pos_;

        // Returns the current character or '\0' if at or past end of input.
        // Invariant: pos_ <= input_.size()
        char        current() const {
            return pos_ < input_.size() ? input_[pos_] : '\0';
        }
        // Returns the next character or '\0' if at or past end of input.
        char peek() const {
            return pos_ + 1 < input_.size() ? input_[pos_ + 1] : '\0';
        }
        void advance() { pos_++; }

        // Advances pos_ past any contiguous ASCII whitespace.
        // Assumes input_ is valid UTF-8; only ASCII whitespace is skipped.
        void skip_whitespace() {
            while (std::isspace(static_cast<unsigned char>(current()))) {
                advance();
            }
        }

        public:
        explicit CommandLexer(const std::string& input)
            : input_(input), pos_(0) {}

        // Tokenizes the next logical unit from the input stream.
        // - Quoted strings are parsed as a single token; no escape handling.
        // - Commands must begin with ':' and are read until whitespace.
        // - Flags must begin with '-' or '--' and are distinguished from
        // negative numbers.
        // - Words are maximal runs of non-whitespace, non-quote characters.
        // Returns Eof token if input is exhausted.
        CommandToken next_token() {
            skip_whitespace();
            size_t start = pos_;

            if (current() == '\0') {
                return CommandToken(CommandTokenType::Eof, "", start, pos_);
            }

            char c = current();

            // Quoted string: consumes until next '"' or end of input.
            // No support for escaped quotes; input is assumed well-formed.
            if (c == '"') {
                advance();
                std::string val;
                while (current() != '\0' && current() != '"') {
                    val += current();
                    advance();
                }
                if (current() == '"')
                    advance();
                return CommandToken(
                    CommandTokenType::QuotedString, val, start, pos_);
            }

            // Command: must start with ':' and continues until whitespace.
            // Used to distinguish command invocations from arguments.
            if (c == ':') {
                std::string val;
                while (current() != '\0' &&
                       !std::isspace(static_cast<unsigned char>(current()))) {
                    val += current();
                    advance();
                }
                return CommandToken(
                    CommandTokenType::Command, val, start, pos_);
            }

            // Flag: must start with '-' or '--' and be followed by alpha or
            // another '-'. This avoids misclassifying negative numbers as
            // flags.
            if (c == '-' && (std::isalpha(static_cast<unsigned char>(peek())) ||
                             peek() == '-')) {
                std::string val;
                while (current() != '\0' &&
                       !std::isspace(static_cast<unsigned char>(current()))) {
                    val += current();
                    advance();
                }
                return CommandToken(CommandTokenType::Flag, val, start, pos_);
            }

            // Word: maximal run of non-whitespace, non-quote characters.
            // Used for general arguments and operands.
            std::string val;
            while (current() != '\0' &&
                   !std::isspace(static_cast<unsigned char>(current())) &&
                   current() != '"') {
                val += current();
                advance();
            }
            return CommandToken(CommandTokenType::Word, val, start, pos_);
        }

        // Consumes and returns the remainder of the input, trimming trailing
        // whitespace. Used for cases where the rest of the input is a single
        // logical unit (e.g., math expressions). Advances pos_ to end of input.
        std::string consume_rest() {
            skip_whitespace();
            if (pos_ >= input_.size())
                return "";

            std::string rest = input_.substr(pos_);

            // Right-trim ASCII whitespace.
            size_t      end  = rest.find_last_not_of(" \t\r\n");
            if (end != std::string::npos) {
                rest = rest.substr(0, end + 1);
            } else {
                rest.clear();
            }

            pos_ = input_.size();
            return rest;
        }
    };

} // namespace math_solver

#endif // MATH_SOLVER_CMD_LEXER_HPP