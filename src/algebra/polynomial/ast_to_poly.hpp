#pragma once

#include "ast/math/binary_expr.hpp"
#include "ast/math/expr_visitor.hpp"
#include "ast/math/number_expr.hpp"
#include "ast/math/variable_expr.hpp"
#include "core/error.hpp"
#include "polynomial.hpp"
#include <cmath>

namespace math_solver {

    constexpr double kEpsilon = 1e-12;

    // Converts an Expr AST to a Polynomial, rejecting non-polynomial
    // constructs.
    //
    // This is a strict lowering pass: only ASTs that can be exactly represented
    // as a univariate or multivariate polynomial are accepted. Any operation
    // that would introduce non-polynomial structure (e.g., variable division,
    // negative or fractional exponents) is rejected with a precise error.
    //
    // Correctness notes:
    //   - The visitor is stateless except for `result_`, which is overwritten
    //     on each visit. Recursive calls instantiate new visitors to avoid
    //     accidental state leakage.
    //   - All error cases are checked before constructing the resulting
    //   Polynomial.
    //   - Exponentiation is only allowed for non-negative integer constants.
    //   - Division is only allowed by nonzero constants.
    //
    // Performance: This is not optimized for speed; it is intended for
    // correctness and clear error reporting. If used in hot paths, consider
    // memoization or iterative traversal.
    class ASTToPolynomial : public ExprVisitor {
        private:
        Polynomial  result_;
        std::string input_;

        public:
        explicit ASTToPolynomial(const std::string& input = "")
            : input_(input) {}

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
            // Lower left operand and store result
            node.left().accept(*this);
            Polynomial left = result_;

            // Lower right operand and store result
            node.right().accept(*this);
            Polynomial right = result_;

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
                // Only allow division by a constant; reject variable
                // denominators.
                if (!right.is_constant()) {
                    throw MathException(errors::polynomial(
                        "cannot divide by a variable expression",
                        node.right().span(), input_));
                }
                // Eagerly check for division by zero to avoid undefined
                // behavior.
                if (std::abs(right.constant_value()) < kEpsilon) {
                    throw MathException(errors::polynomial(
                        "division by zero", node.right().span(), input_));
                }
                result_ = left / right.constant_value();
                break;

            case BinaryOpType::Pow: {
                // Only allow exponentiation by non-negative integer constants.
                if (!right.is_constant()) {
                    throw MathException(errors::polynomial(
                        "exponent must be a non-negative integer constant",
                        node.right().span(), input_));
                }
                double exp_val = right.constant_value();
                int    exp_int = static_cast<int>(std::round(exp_val));
                // Reject fractional or negative exponents; only allow integer
                // >= 0.
                if (std::abs(exp_val - exp_int) > kEpsilon || exp_int < 0) {
                    throw MathException(errors::polynomial(
                        "exponent must be a non-negative integer",
                        node.right().span(), input_));
                }
                result_ = left.pow(exp_int);
                break;
            }
            }
        }
    };

} // namespace math_solver
