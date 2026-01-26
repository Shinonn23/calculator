#include "parser.h"
#include "../ast/expr.h"

namespace math_solver {

    ExprPtr Parser::parse_primary() {
        if (current_.type == TokenType::LParen) {
            advance();
            auto expr = parse_expression();
            expect(TokenType::RParen, "Expected ')'");
            return expr;
        }

        if (current_.type == TokenType::Number) {
            double val = current_.value;
            advance();
            return std::make_unique<Number>(val);
        }

        throw std::runtime_error("Unexpected token: " +
                                 std::string(token_type_name(current_.type)));
    }

    ExprPtr Parser::parse_unary() {
        if (current_.type == TokenType::Minus) {
            advance();
            ExprPtr expr = parse_unary();
            return std::make_unique<BinaryOp>(std::make_unique<Number>(0),
                                              std::move(expr),
                                              BinaryOpType::Sub);
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
            advance();
            auto right = parse_unary();
            left = std::make_unique<BinaryOp>(std::move(left), std::move(right),
                                              BinaryOpType::Pow);
        }

        return left;
    }

    ExprPtr Parser::parse_multiplicative() {
        auto left = parse_power();

        while (current_.type == TokenType::Mul ||
               current_.type == TokenType::Div) {
            BinaryOpType op = (current_.type == TokenType::Mul)
                                  ? BinaryOpType::Mul
                                  : BinaryOpType::Div;
            advance();
            auto right = parse_power();
            left = std::make_unique<BinaryOp>(std::move(left), std::move(right),
                                              op);
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
            auto right = parse_multiplicative();
            left = std::make_unique<BinaryOp>(std::move(left), std::move(right),
                                              op);
        }

        return left;
    }

    ExprPtr Parser::parse_expression() { return parse_additive(); }

} // namespace math_solver