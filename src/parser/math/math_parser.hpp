#ifndef PARSER_H
#define PARSER_H

#include "ast/math/equation_expr.hpp"
#include "ast/math/expr.hpp"
#include "core/error.hpp"
#include "lexer/lexer.hpp"
#include "lexer/token.hpp"
#include <string>

namespace math_solver {

    class Parser {
        private:
        Lexer       lexer_;
        Token       current_;
        std::string input_;
        // bool        is_implicit_;

        void        advance() { current_ = lexer_.next_token(); }

        void        expect(TokenType type, const std::string& msg) {
            if (current_.type != type) {
                throw ParseError(msg, current_.span, input_);
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
        explicit Parser(const std::string& input)
            : lexer_(input), input_(input) {
            current_ = lexer_.next_token();
        }
        const std::string&              input() const;
        ExprPtr                         parse();
        std::pair<ExprPtr, EquationPtr> parse_expression_or_equation();
        EquationPtr                     parse_equation();
    };
} // namespace math_solver

#endif
