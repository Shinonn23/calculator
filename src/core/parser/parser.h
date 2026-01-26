#ifndef PARSER_H
#define PARSER_H

#include "../ast/binary.h"
#include "../ast/expr.h"
#include "../ast/number.h"
#include "../lexer/lexer.h"
#include "../lexer/token.h"
#include <memory>
#include <stdexcept>
#include <string>

namespace math_solver {

    class Parser {
        private:
        Lexer lexer_;
        Token current_;

        void  advance() { current_ = lexer_.next_token(); }

        void  expect(TokenType type, const std::string& msg) {
            if (current_.type != type) {
                throw std::runtime_error(msg);
            }
            advance();
        }

        // Parsing functions (precedence climbing)
        ExprPtr parse_primary();
        ExprPtr parse_unary();
        ExprPtr parse_power();
        ExprPtr parse_multiplicative();
        ExprPtr parse_additive();
        ExprPtr parse_expression();

        public:
        explicit Parser(const std::string& input) : lexer_(input) {
            current_ = lexer_.next_token();
        }

        ExprPtr parse() {
            auto expr = parse_expression();
            if (current_.type != TokenType::End) {
                throw std::runtime_error("Unexpected input after expression");
            }
            return expr;
        }
    };

} // namespace math_solver

#endif
