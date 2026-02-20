#pragma once

#include "core/error.hpp"
#include "core/span.hpp"
#include "math_token.hpp"
#include <cctype>
#include <string>

namespace math_solver {

    class Lexer {
        private:
        std::string input_;
        size_t      pos_;

        // Returns the current character or '\0' if at or past end of input.
        // Invariant: pos_ <= input_.size().
        char        current() const {
            return pos_ < input_.size() ? input_[pos_] : '\0';
        }

        // Advances the cursor by one character. No bounds checks.
        // Callers must ensure pos_ < input_.size() or handle '\0' sentinel.
        void advance() { pos_++; }

        // Skips over contiguous ASCII whitespace.
        // Assumes input is valid UTF-8; does not handle non-ASCII whitespace.
        void skip_whitespace() {
            while (std::isspace(static_cast<unsigned char>(current())))
                advance();
        }

        // Accepts ASCII alphabetic or '_'. Used for identifier start.
        // Assumes input is valid UTF-8; non-ASCII identifiers are rejected.
        bool is_identifier_start(char c) const {
            return std::isalpha(static_cast<unsigned char>(c)) || c == '_';
        }

        // Accepts ASCII alphanumeric or '_'. Used for identifier continuation.
        bool is_identifier_char(char c) const {
            return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
        }

        public:
        explicit Lexer(const std::string& input) : input_(input), pos_(0) {}

        const std::string& input() const { return input_; }
        size_t             position() const { return pos_; }

        // Main tokenization entry point.
        // Returns the next token, advancing internal state.
        // Throws on malformed input or reserved keyword usage.
        //
        // Invariants:
        // - pos_ always points to the next unconsumed character.
        // - On error, pos_ is advanced to the error span end.
        // - End-of-input is signaled by TokenType::End.
        //
        // Performance: Single-pass, no backtracking.
        Token              next_token() {
            skip_whitespace();

            size_t start = pos_;

            if (current() == '\0') {
                // End-of-input sentinel. No further tokens will be produced.
                return Token(TokenType::End, 0, Span(pos_, pos_));
            }

            // Fast path for numeric literals.
            // Accepts leading '.' for floats (e.g., ".5"), but rejects lone
            // '.'. Only a single '.' is permitted per literal.
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

                // Rejects empty or invalid numbers (e.g., ".").
                if (num.empty() || num == ".") {
                    throw ParseError(
                        "invalid number", Span(start, pos_), input_);
                }

                return Token(
                    TokenType::Number, std::stod(num), Span(start, pos_));
            }

            // Identifier/keyword path.
            // Only ASCII identifiers are accepted.
            // Reserved keywords are rejected with a dedicated error.
            if (is_identifier_start(current())) {
                std::string name;
                while (is_identifier_char(current())) {
                    name += current();
                    advance();
                }

                if (is_reserved_keyword(name)) {
                    throw ReservedKeywordError(name, Span(start, pos_), input_);
                }

                return Token(TokenType::Identifier, name, Span(start, pos_));
            }

            // Single-character operator dispatch.
            // No lookahead; multi-char operators are not supported.
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

            // Any unrecognized character is treated as a hard error.
            // No recovery attempted; caller must handle.
            throw ParseError("unexpected character '" + std::string(1, c) + "'",
                             span,
                             input_);
        }
    };

} // namespace math_solver
