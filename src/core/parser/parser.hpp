#ifndef PARSER_H
#define PARSER_H

#include "../ast/equation.hpp"
#include "../ast/expr.hpp"
#include "../common/error.hpp"
#include "../lexer/lexer.hpp"
#include "../lexer/token.hpp"
#include <memory>
#include <string>

namespace math_solver {

    class Parser {
        private:
        Lexer       lexer_;
        Token       current_;
        std::string input_;
        bool        is_implicit_;

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

        const std::string& input() const { return input_; }

        // Parse a simple expression (no equation)
        ExprPtr            parse() {
            auto expr = parse_expression();
            if (current_.type != TokenType::End) {
                throw ParseError("unexpected input after expression",
                                            current_.span, input_);
            }
            return expr;
        }

        // Parse either an equation (lhs = rhs) or a simple expression
        // Returns: pair<ExprPtr, EquationPtr> where exactly one is non-null
        std::pair<ExprPtr, EquationPtr> parse_expression_or_equation() {
            auto lhs = parse_expression();

            if (current_.type == TokenType::Equals) {
                Span equals_span = current_.span;
                advance();

                if (current_.type == TokenType::End) {
                    throw ParseError("expected expression after '='",
                                     equals_span, input_);
                }

                auto rhs = parse_expression();

                if (current_.type != TokenType::End) {
                    throw ParseError("unexpected input after equation",
                                     current_.span, input_);
                }

                Span eq_span = lhs->span().merge(rhs->span());
                return {nullptr, std::make_unique<Equation>(
                                     std::move(lhs), std::move(rhs), eq_span)};
            }

            if (current_.type != TokenType::End) {
                throw ParseError("unexpected input after expression",
                                 current_.span, input_);
            }

            return {std::move(lhs), nullptr};
        }

        // Parse equation only (throws if not an equation)
        EquationPtr parse_equation() {
            auto lhs = parse_expression();

            if (current_.type != TokenType::Equals) {
                throw ParseError("expected '=' for equation", current_.span,
                                 input_);
            }

            Span equals_span = current_.span;
            advance();

            if (current_.type == TokenType::End) {
                throw ParseError("expected expression after '='", equals_span,
                                 input_);
            }

            auto rhs = parse_expression();

            if (current_.type != TokenType::End) {
                throw ParseError("unexpected input after equation",
                                 current_.span, input_);
            }

            return std::make_unique<Equation>(std::move(lhs), std::move(rhs));
        }
    };

} // namespace math_solver

#endif
