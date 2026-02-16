#ifndef AST_TO_POLY_H
#define AST_TO_POLY_H

#include "ast/binary.hpp"
#include "ast/expr.hpp"
#include "ast/number.hpp"
#include "ast/variable.hpp"
#include "common/error.hpp"
#include "polynomial.hpp"
#include <cmath>

namespace math_solver {

    // ========================================================================
    // PolynomialError — thrown when an expression cannot be represented
    // as a polynomial (e.g. division by variable, fractional exponent)
    // ========================================================================
    class PolynomialError : public MathError {
        public:
        PolynomialError(const std::string& message, const Span& span = Span(),
                        const std::string& input = "")
            : MathError(message, span, input) {}
    };

    // ========================================================================
    // AST → Polynomial converter
    // Walks the AST using the Visitor pattern and builds a Polynomial
    // ========================================================================
    class ASTToPolynomial : public ExprVisitor {
        private:
        Polynomial  result_;
        std::string input_;

        public:
        explicit ASTToPolynomial(const std::string& input = "") : input_(input) {}

        Polynomial convert(const Expr& expr) {
            expr.accept(*this);
            return result_;
        }

        void visit(const Number& node) override {
            result_ = Polynomial(node.value());
        }

        void visit(const Variable& node) override {
            result_ = Polynomial(1.0, node.name(), 1);
        }

        void visit(const BinaryOp& node) override {
            ASTToPolynomial left_conv(input_);
            Polynomial      left = left_conv.convert(node.left());

            ASTToPolynomial right_conv(input_);
            Polynomial      right = right_conv.convert(node.right());

            switch (node.op()) {
            case BinaryOpType::Add:
                result_ = left + right;
                break;

            case BinaryOpType::Sub:
                result_ = left - right;
                break;

            case BinaryOpType::Mul:
                result_ = left * right;
                break;

            case BinaryOpType::Div:
                if (!right.is_constant()) {
                    throw PolynomialError(
                        "cannot divide by a variable expression",
                        node.right().span(), input_);
                }
                if (std::abs(right.constant_value()) < 1e-12) {
                    throw PolynomialError("division by zero",
                                          node.right().span(), input_);
                }
                result_ = left / right.constant_value();
                break;

            case BinaryOpType::Pow: {
                if (!right.is_constant()) {
                    throw PolynomialError(
                        "exponent must be a non-negative integer constant",
                        node.right().span(), input_);
                }
                double exp_val = right.constant_value();
                int    exp_int = static_cast<int>(std::round(exp_val));
                if (std::abs(exp_val - exp_int) > 1e-9 || exp_int < 0) {
                    throw PolynomialError(
                        "exponent must be a non-negative integer",
                        node.right().span(), input_);
                }
                result_ = left.pow(exp_int);
                break;
            }
            }
        }
    };

} // namespace math_solver

#endif
