#include "parser.h"
#include "ast/binary.h"
#include "ast/expr.h"
#include "ast/function_call.h"
#include "ast/index_access.h"
#include "ast/number.h"
#include "ast/number_array.h"
#include "ast/variable.h"

namespace math_solver {

    // Parse postfix operators like [index]
    ExprPtr Parser::parse_postfix(ExprPtr expr) {
        while (current_.type == TokenType::LBracket) {
            Span bracket_span = current_.span;
            advance();

            // Expect a non-negative integer index
            if (current_.type != TokenType::Number) {
                throw ParseError("expected integer index inside []",
                                 current_.span, input_);
            }

            double idx_val = current_.value;
            // Validate it's a non-negative integer
            if (idx_val < 0 || idx_val != std::floor(idx_val)) {
                throw ParseError("array index must be a non-negative integer",
                                 current_.span, input_);
            }
            size_t index = static_cast<size_t>(idx_val);
            advance();

            if (current_.type != TokenType::RBracket) {
                throw ParseError("expected ']'", current_.span, input_);
            }
            Span end_span = current_.span;
            advance();

            Span access_span = expr->span().merge(end_span);
            expr = std::make_unique<IndexAccess>(std::move(expr), index,
                                                 access_span);
        }
        return expr;
    }

    // Try to extract a numeric value from a parsed expression.
    // Handles Number nodes and unary-minus pattern (0 - Number).
    static bool try_extract_number(const Expr* expr, double& out) {
        if (auto* num = dynamic_cast<const Number*>(expr)) {
            out = num->value();
            return true;
        }
        // Unary minus is represented as BinaryOp(0, x, Sub)
        if (auto* bin = dynamic_cast<const BinaryOp*>(expr)) {
            if (bin->op() == BinaryOpType::Sub) {
                auto* lhs = dynamic_cast<const Number*>(&bin->left());
                auto* rhs = dynamic_cast<const Number*>(&bin->right());
                if (lhs && rhs && lhs->value() == 0.0) {
                    out = -rhs->value();
                    return true;
                }
            }
        }
        return false;
    }

    ExprPtr Parser::parse_primary() {
        // Array literal: [expr, expr, ...]
        if (current_.type == TokenType::LBracket) {
            Span start_span = current_.span;
            advance();

            std::vector<double> values;

            if (current_.type != TokenType::RBracket) {
                auto   elem = parse_expression();
                double v    = 0.0;
                if (!try_extract_number(elem.get(), v)) {
                    throw ParseError(
                        "array literal elements must be numeric constants",
                        elem->span(), input_);
                }
                values.push_back(v);

                while (current_.type == TokenType::Comma) {
                    advance();
                    elem = parse_expression();
                    if (!try_extract_number(elem.get(), v)) {
                        throw ParseError(
                            "array literal elements must be numeric "
                            "constants",
                            elem->span(), input_);
                    }
                    values.push_back(v);
                }
            }

            if (current_.type != TokenType::RBracket) {
                throw ParseError("expected ']'", current_.span, input_);
            }
            Span end_span = current_.span;
            advance();

            Span arr_span = start_span.merge(end_span);
            auto expr     = std::make_unique<NumberArray>(values, arr_span);
            return parse_postfix(std::move(expr));
        }

        if (current_.type == TokenType::LParen) {
            Span start_span = current_.span;
            advance();
            auto expr = parse_expression();
            if (current_.type != TokenType::RParen) {
                throw ParseError("expected ')'", current_.span, input_);
            }
            Span end_span = current_.span;
            advance();
            expr->set_span(start_span.merge(end_span));
            return parse_postfix(std::move(expr));
        }

        if (current_.type == TokenType::Number) {
            double val  = current_.value;
            Span   span = current_.span;
            advance();
            auto expr = std::make_unique<Number>(val, span);
            return parse_postfix(std::move(expr));
        }

        if (current_.type == TokenType::Identifier) {
            std::string name = current_.name;
            Span        span = current_.span;
            advance();

            // Check if this is a function call: identifier followed by '('
            if (is_builtin_function(name) &&
                current_.type == TokenType::LParen) {
                advance();

                std::vector<ExprPtr> args;
                if (current_.type != TokenType::RParen) {
                    args.push_back(parse_expression());
                    while (current_.type == TokenType::Comma) {
                        advance();
                        args.push_back(parse_expression());
                    }
                }

                if (current_.type != TokenType::RParen) {
                    throw ParseError("expected ')' after function arguments",
                                     current_.span, input_);
                }
                Span end_span = current_.span;
                advance();

                Span func_span = span.merge(end_span);
                auto expr      = std::make_unique<FunctionCall>(
                    name, std::move(args), func_span);
                return parse_postfix(std::move(expr));
            }

            auto expr = std::make_unique<Variable>(name, span);
            return parse_postfix(std::move(expr));
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
            // bool is_implicit = false;

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

    ExprPtr Parser::parse_expression() { return parse_additive(); }

} // namespace math_solver
