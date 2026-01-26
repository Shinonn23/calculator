#ifndef LEXER_H
#define LEXER_H

#include "token.h"
#include <cctype>
#include <stdexcept>
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
            while (std::isspace(current()))
                advance();
        }

        public:
        explicit Lexer(const std::string& input) : input_(input), pos_(0) {}

        Token next_token() {
            skip_whitespace();

            if (current() == '\0') {
                return {TokenType::End, 0};
            }

            // Numbers
            if (std::isdigit(current()) || current() == '.') {
                std::string num;
                bool        has_dot = false;

                while (std::isdigit(current()) ||
                       (current() == '.' && !has_dot)) {
                    if (current() == '.')
                        has_dot = true;
                    num += current();
                    advance();
                }

                if (num.empty() || num == ".") {
                    throw std::runtime_error("Invalid number");
                }

                return {TokenType::Number, std::stod(num)};
            }

            // Operators
            char c = current();
            advance();

            switch (c) {
            case '+':
                return {TokenType::Plus, 0};
            case '-':
                return {TokenType::Minus, 0};
            case '*':
                return {TokenType::Mul, 0};
            case '/':
                return {TokenType::Div, 0};
            case '^':
                return {TokenType::Pow, 0};
            case '(':
                return {TokenType::LParen, 0};
            case ')':
                return {TokenType::RParen, 0};
            }

            throw std::runtime_error("Unexpected character: " +
                                     std::string(1, c));
        }
    };

} // namespace math_solver

#endif