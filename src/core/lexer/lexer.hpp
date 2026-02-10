#ifndef LEXER_H
#define LEXER_H

#include "../common/error.hpp"
#include "../common/span.hpp"
#include "token.hpp"
#include <cctype>
#include <string>

namespace math_solver {

    class Lexer {
        private:
        std::string input_;
        size_t      pos_;

        char        current() const {
            return pos_ < input_.size() ? input_[pos_] : '\0';
        }

        void advance() { pos_++; }

        void skip_whitespace() {
            while (std::isspace(static_cast<unsigned char>(current())))
                advance();
        }

        bool is_identifier_start(char c) const {
            return std::isalpha(static_cast<unsigned char>(c)) || c == '_';
        }

        bool is_identifier_char(char c) const {
            return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
        }

        public:
        explicit Lexer(const std::string& input) : input_(input), pos_(0) {}

        const std::string& input() const { return input_; }
        size_t             position() const { return pos_; }

        Token              next_token() {
            skip_whitespace();

            size_t start = pos_;

            if (current() == '\0') {
                return Token(TokenType::End, 0, Span(pos_, pos_));
            }

            // Numbers
            if (std::isdigit(static_cast<unsigned char>(current())) ||
                current() == '.') {
                std::string num;
                bool        has_dot = false;

                while (std::isdigit(static_cast<unsigned char>(current())) ||
                       (current() == '.' && !has_dot)) {
                    if (current() == '.')
                        has_dot = true;
                    num += current();
                    advance();
                }

                if (num.empty() || num == ".") {
                    throw ParseError("invalid number", Span(start, pos_),
                                                  input_);
                }

                return Token(TokenType::Number, std::stod(num),
                                          Span(start, pos_));
            }

            // Identifiers (variables and keywords)
            if (is_identifier_start(current())) {
                std::string name;
                while (is_identifier_char(current())) {
                    name += current();
                    advance();
                }

                // Check for reserved keywords
                if (is_reserved_keyword(name)) {
                    throw ReservedKeywordError(name, Span(start, pos_), input_);
                }

                return Token(TokenType::Identifier, name, Span(start, pos_));
            }

            // Single character operators
            char c = current();
            advance();
            Span span(start, pos_);

            switch (c) {
            case '+':
                return Token(TokenType::Plus, 0, span);
            case '-':
                return Token(TokenType::Minus, 0, span);
            case '*':
                return Token(TokenType::Mul, 0, span);
            case '/':
                return Token(TokenType::Div, 0, span);
            case '^':
                return Token(TokenType::Pow, 0, span);
            case '(':
                return Token(TokenType::LParen, 0, span);
            case ')':
                return Token(TokenType::RParen, 0, span);
            case '=':
                return Token(TokenType::Equals, 0, span);
            }

            throw ParseError("unexpected character '" + std::string(1, c) + "'",
                                          span, input_);
        }
    };

} // namespace math_solver

#endif