#include "math_parser.hpp"
#include "ast/binary.hpp"
#include "ast/expr.hpp"
#include "ast/number.hpp"
#include "ast/variable.hpp"

#include <memory>

namespace math_solver {

    // Private parsing functions (precedence climbing)
    ExprPtr Parser::parse_primary() {
        if (current_.type == TokenType::LParen) {
            Span start_span = current_.span;
            advance();
            auto expr = parse_expression();
            if (current_.type != TokenType::RParen) {
                throw ParseError("expected ')'", current_.span, input_);
            }
            Span end_span = current_.span;
            advance();
            // Set span to include parentheses
            expr->set_span(start_span.merge(end_span));
            return expr;
        }

        if (current_.type == TokenType::Number) {
            double val  = current_.value;
            Span   span = current_.span;
            advance();
            return std::make_unique<Number>(val, span);
        }

        if (current_.type == TokenType::Identifier) {
            std::string name = current_.name;
            Span        span = current_.span;
            advance();
            return std::make_unique<Variable>(name, span);
        }

        throw ParseError("unexpected token '" +
                             std::string(token_type_name(current_.type)) + "'",
                         current_.span, input_);
    }

    ExprPtr Parser::parse_unary() {
        if (current_.type == TokenType::Minus) {
            Span op_span = current_.span;
            advance();
            ExprPtr expr        = parse_unary();
            // Represent -x as (0 - x)
            auto    zero        = std::make_unique<Number>(0, op_span);
            Span    result_span = op_span.merge(expr->span());
            return std::make_unique<BinaryOp>(std::move(zero), std::move(expr),
                                              BinaryOpType::Sub, result_span);
        }
        if (current_.type == TokenType::Plus) {
            advance();
            return parse_unary();
        }
        return parse_primary();
    }

    ExprPtr Parser::parse_power() {
        auto left = parse_unary();

        while (current_.type == TokenType::Pow) {
            // Span op_span = current_.span;
            advance();
            auto right       = parse_unary();
            Span result_span = left->span().merge(right->span());
            left = std::make_unique<BinaryOp>(std::move(left), std::move(right),
                                              BinaryOpType::Pow, result_span);
        }

        return left;
    }

    ExprPtr Parser::parse_multiplicative() {
        auto left = parse_power();

        // Handle both explicit (* /) and implicit multiplication (5x, 2(x+1),
        // etc.)
        while (current_.type == TokenType::Mul ||
               current_.type == TokenType::Div ||
               current_.type == TokenType::Number ||
               current_.type == TokenType::Identifier ||
               current_.type == TokenType::LParen) {

            // Determine if it's explicit or implicit multiplication
            BinaryOpType op;
            // bool         is_implicit = false;

            if (current_.type == TokenType::Mul) {
                op = BinaryOpType::Mul;
                advance();
            } else if (current_.type == TokenType::Div) {
                op = BinaryOpType::Div;
                advance();
            } else {
                // Implicit multiplication: number, identifier, or ( follows
                op = BinaryOpType::Mul;
                // is_implicit = true;
                // Don't advance - the next parse_power will consume the token
            }

            auto right       = parse_power();
            Span result_span = left->span().merge(right->span());
            left = std::make_unique<BinaryOp>(std::move(left), std::move(right),
                                              op, result_span);
        }

        return left;
    }

    ExprPtr Parser::parse_additive() {
        auto left = parse_multiplicative();

        while (current_.type == TokenType::Plus ||
               current_.type == TokenType::Minus) {
            BinaryOpType op = (current_.type == TokenType::Plus)
                                  ? BinaryOpType::Add
                                  : BinaryOpType::Sub;
            advance();
            auto right       = parse_multiplicative();
            Span result_span = left->span().merge(right->span());
            left = std::make_unique<BinaryOp>(std::move(left), std::move(right),
                                              op, result_span);
        }

        return left;
    }

    ExprPtr            Parser::parse_expression() { return parse_additive(); }

    // Public interface
    const std::string& Parser::input() const { return input_; }

    // Parse a simple expression (no equation)
    ExprPtr            Parser::parse() {
        auto expr = parse_expression();
        if (current_.type != TokenType::End) {
            throw ParseError("unexpected input after expression", current_.span,
                                        input_);
        }
        return expr;
    }

    // Parse either an equation (lhs = rhs) or a simple expression
    // Returns: pair<ExprPtr, EquationPtr> where exactly one is non-null
    std::pair<ExprPtr, EquationPtr> Parser::parse_expression_or_equation() {
        auto lhs = parse_expression();

        if (current_.type == TokenType::Equals) {
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

            Span eq_span = lhs->span().merge(rhs->span());
            return {nullptr, std::make_unique<Equation>(
                                 std::move(lhs), std::move(rhs), eq_span)};
        }

        if (current_.type != TokenType::End) {
            throw ParseError("unexpected input after expression", current_.span,
                             input_);
        }

        return {std::move(lhs), nullptr};
    }

    // Parse equation only (throws if not an equation)
    EquationPtr Parser::parse_equation() {
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
            throw ParseError("unexpected input after equation", current_.span,
                             input_);
        }

        return std::make_unique<Equation>(std::move(lhs), std::move(rhs));
    };

} // namespace math_solver