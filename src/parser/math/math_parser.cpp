#include "math_parser.hpp"
#include "ast/math/binary_expr.hpp"
#include "ast/math/expr.hpp"
#include "ast/math/number_expr.hpp"
#include "ast/math/variable_expr.hpp"

#include <memory>

namespace math_solver {

    // Parses a primary expression.
    // - Handles grouping via parentheses, numbers, and identifiers.
    // - Parentheses must be balanced; span is extended to include them.
    // - Throws on unexpected tokens; assumes caller has advanced to a valid
    // start.
    Result<ExprPtr> Parser::parse_primary() {
        if (current_.type == TokenType::LParen) {
            Span start_span = current_.span;
            (void)advance();
            auto expr = parse_expression();
            if (!expr)
                return Result<ExprPtr>::err(expr.error());
            if (current_.type != TokenType::RParen)
                return Result<ExprPtr>::err(
                    errors::parse("expected ')'", current_.span, input_));
            Span end_span = current_.span;
            (void)advance();
            (*expr)->set_span(start_span.merge(end_span));
            return Result<ExprPtr>::ok(std::move(*expr));
        }

        if (current_.type == TokenType::Number) {
            double val  = current_.value;
            Span   span = current_.span;
            (void)advance();
            return Result<ExprPtr>::ok(std::make_unique<Number>(val, span));
        }

        if (current_.type == TokenType::Identifier) {
            std::string name = current_.name;
            Span        span = current_.span;
            (void)advance();
            return Result<ExprPtr>::ok(std::make_unique<Variable>(name, span));
        }

        return Result<ExprPtr>::err(
            errors::parse("unexpected token '" +
                              std::string(token_type_name(current_.type)) + "'",
                          current_.span, input_));
    }

    // Handles unary prefix operators.
    // - Only supports unary minus and plus.
    // - For minus, desugars to (0 - x) to simplify downstream handling.
    // - Recursively parses further unary operators for correct associativity.
    Result<ExprPtr> Parser::parse_unary() {
        if (current_.type == TokenType::Minus) {
            Span op_span = current_.span;
            (void)advance();
            auto expr = parse_unary();
            if (!expr)
                return Result<ExprPtr>::err(expr.error());
            auto zero        = std::make_unique<Number>(0, op_span);
            Span result_span = op_span.merge((*expr)->span());
            return Result<ExprPtr>::ok(
                std::make_unique<BinaryOp>(std::move(zero), std::move(*expr),
                                           BinaryOpType::Sub, result_span));
        }
        if (current_.type == TokenType::Plus) {
            (void)advance();
            return parse_unary();
        }
        return parse_primary();
    }

    // Parses right-associative exponentiation.
    // - Repeatedly parses chains of '^' operators using recursion to ensure
    // right-associativity.
    Result<ExprPtr> Parser::parse_power() {
        auto left = parse_unary();
        if (!left)
            return Result<ExprPtr>::err(left.error());

        if (current_.type == TokenType::Pow) {
            (void)advance();
            auto right = parse_power();
            if (!right)
                return Result<ExprPtr>::err(right.error());
            Span result_span = (*left)->span().merge((*right)->span());
            return Result<ExprPtr>::ok(
                std::make_unique<BinaryOp>(std::move(*left), std::move(*right),
                                           BinaryOpType::Pow, result_span));
        }

        return Result<ExprPtr>::ok(std::move(*left));
    }

    // Handles multiplicative precedence, including implicit multiplication.
    // - Accepts explicit '*' and '/' as well as implicit cases:
    //   juxtaposed numbers, identifiers, or parentheses.
    // - Implicit multiplication is treated identically to explicit '*'.
    // - Invariant: parse_power() is called for each right operand to ensure
    //   correct precedence for exponentiation.
    // - This design allows for expressions like "2x" or "3(x+1)" without
    // ambiguity.
    Result<ExprPtr> Parser::parse_multiplicative() {
        auto left = parse_power();
        if (!left)
            return Result<ExprPtr>::err(left.error());

        while (current_.type == TokenType::Mul ||
               current_.type == TokenType::Div ||
               current_.type == TokenType::Number ||
               current_.type == TokenType::Identifier ||
               current_.type == TokenType::LParen) {

            BinaryOpType op;

            if (current_.type == TokenType::Mul) {
                op = BinaryOpType::Mul;
                advance();
            } else if (current_.type == TokenType::Div) {
                op = BinaryOpType::Div;
                advance();
            } else {
                op = BinaryOpType::Mul;
                // Do not advance; next parse_power() will consume the token.
            }

            auto right = parse_power();
            if (!right)
                return Result<ExprPtr>::err(right.error());
            Span result_span = (*left)->span().merge((*right)->span());
            left             = Result<ExprPtr>::ok(std::make_unique<BinaryOp>(
                std::move(*left), std::move(*right), op, result_span));
        }

        return Result<ExprPtr>::ok(std::move(*left));
    }

    // Handles additive precedence.
    // - Left-associative parsing of '+' and '-' operators.
    // - Each right operand is parsed at multiplicative precedence.
    // - Invariant: No further tokens of higher precedence remain after this
    // point.
    Result<ExprPtr> Parser::parse_additive() {
        auto left = parse_multiplicative();
        if (!left)
            return Result<ExprPtr>::err(left.error());

        while (current_.type == TokenType::Plus ||
               current_.type == TokenType::Minus) {
            BinaryOpType op = (current_.type == TokenType::Plus)
                                  ? BinaryOpType::Add
                                  : BinaryOpType::Sub;
            (void)advance();
            auto right = parse_multiplicative();
            if (!right)
                return Result<ExprPtr>::err(right.error());
            Span result_span = (*left)->span().merge((*right)->span());
            left             = Result<ExprPtr>::ok(std::make_unique<BinaryOp>(
                std::move(*left), std::move(*right), op, result_span));
        }

        return Result<ExprPtr>::ok(std::move(*left));
    }

    // Entry point for parsing a full expression.
    // - All operator precedence and associativity handled by lower layers.
    Result<ExprPtr>    Parser::parse_expression() { return parse_additive(); }

    const std::string& Parser::input() const { return input_; }

    // Parses a single expression and ensures no trailing input remains.
    // - Throws if extra tokens are present after the expression.
    // - Used for cases where only a pure expression is valid.
    Result<ExprPtr>    Parser::parse_impl() {
        auto expr = parse_expression();
        if (!expr)
            return Result<ExprPtr>::err(expr.error());
        if (current_.type != TokenType::End)
            return Result<ExprPtr>::err(errors::parse(
                "unexpected input after expression", current_.span, input_));
        return Result<ExprPtr>::ok(std::move(*expr));
    }

    // Parses either an equation (lhs = rhs) or a single expression.
    // - Returns a pair: exactly one of (ExprPtr, EquationPtr) is non-null.
    // - Ensures no trailing input after the parsed construct.
    // - Throws on malformed input or trailing tokens.
    // - Used by higher-level entry points to distinguish between equations and
    // expressions.
    Result<std::pair<ExprPtr, EquationPtr>>
    Parser::parse_expression_or_equation_impl() {
        auto lhs = parse_expression();
        if (!lhs)
            return Result<std::pair<ExprPtr, EquationPtr>>::err(lhs.error());

        if (current_.type == TokenType::Equals) {
            Span equals_span = current_.span;
            (void)advance();

            if (current_.type == TokenType::End) {
                return Result<std::pair<ExprPtr, EquationPtr>>::err(
                    errors::parse("expected expression after '='", equals_span,
                                  input_));
            }

            auto rhs = parse_expression();
            if (!rhs)
                return Result<std::pair<ExprPtr, EquationPtr>>::err(
                    rhs.error());

            if (current_.type != TokenType::End) {
                return Result<std::pair<ExprPtr, EquationPtr>>::err(
                    errors::parse("unexpected input after equation",
                                  current_.span, input_));
            }

            Span eq_span = (*lhs)->span().merge((*rhs)->span());
            return Result<std::pair<ExprPtr, EquationPtr>>::ok(
                {nullptr, std::make_unique<Equation>(
                              std::move(*lhs), std::move(*rhs), eq_span)});
        }

        if (current_.type != TokenType::End) {
            return Result<std::pair<ExprPtr, EquationPtr>>::err(errors::parse(
                "unexpected input after expression", current_.span, input_));
        }

        return Result<std::pair<ExprPtr, EquationPtr>>::ok(
            {std::move(*lhs), nullptr});
    }

    // Parses an equation of the form "lhs = rhs".
    // - Throws if '=' is missing or if trailing input remains.
    // - Used when only equations are valid (e.g., solver entry points).
    // - Invariant: Both sides must be valid expressions.
    Result<EquationPtr> Parser::parse_equation_impl() {
        auto lhs = parse_expression();
        if (!lhs)
            return Result<EquationPtr>::err(lhs.error());

        if (current_.type != TokenType::Equals) {
            return Result<EquationPtr>::err(errors::parse(
                "expected '=' for equation", current_.span, input_));
        }

        Span equals_span = current_.span;
        (void)advance();

        if (current_.type == TokenType::End) {
            return Result<EquationPtr>::err(errors::parse(
                "expected expression after '='", equals_span, input_));
        }

        auto rhs = parse_expression();
        if (!rhs)
            return Result<EquationPtr>::err(rhs.error());

        if (current_.type != TokenType::End) {
            return Result<EquationPtr>::err(errors::parse(
                "unexpected input after equation", current_.span, input_));
        }

        return Result<EquationPtr>::ok(
            std::make_unique<Equation>(std::move(*lhs), std::move(*rhs)));
    }

    Result<ExprPtr> Parser::parse() {
        auto r = parse_impl();
        if (!r)
            return Result<ExprPtr>::err(r.error());
        return Result<ExprPtr>::ok(std::move(*r));
    }

    Result<std::pair<ExprPtr, EquationPtr>>
    Parser::parse_expression_or_equation() {
        return parse_expression_or_equation_impl();
    }

    Result<EquationPtr> Parser::parse_equation() {
        return parse_equation_impl();
    }

} // namespace math_solver