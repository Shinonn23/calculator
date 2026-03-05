#pragma once

//! # Module — `src/algebra/polynomial/ast_to_poly.hpp`
//!
//! Provides `ASTToPolynomial` — a strict AST-to-`Polynomial` lowering pass
//! that walks a math expression tree and rejects any construct that cannot be
//! exactly represented as a multivariate polynomial.
//!
//! Accepted operations: addition, subtraction, multiplication, division by a
//! non-zero constant, exponentiation by a non-negative integer constant, and
//! unary negation. Function calls, array values, variable denominators, and
//! fractional or negative exponents are all rejected with a `Diagnostic`.
//!
//! Used by the `:solve` handler to convert the LHS and RHS of an equation
//! before dispatching to `PolynomialSolver`.

#include "ast/math/array_expr.hpp"
#include "ast/math/binary_expr.hpp"
#include "ast/math/call_expr.hpp"
#include "ast/math/expr_visitor.hpp"
#include "ast/math/number_expr.hpp"
#include "ast/math/unary_expr.hpp"
#include "ast/math/variable_expr.hpp"
#include "diagnostics/diagnostic.hpp"
#include "diagnostics/kinds/polynomial_errors.hpp"
#include "core/tolerance.hpp"
#include "diagnostics/result.hpp"
#include "polynomial.hpp"
#include <cmath>
#include <optional>

namespace math_solver {

    /// Converts a math expression AST to a `Polynomial`, rejecting
    /// non-polynomial constructs with precise diagnostics.
    ///
    /// The visitor is stateless except for `result_` and `error_`, which are
    /// reset on each call to `convert`. Recursive sub-expressions each
    /// instantiate fresh visitor state via `accept`, so there is no accidental
    /// state leakage across sub-trees.
    class ASTToPolynomial : public ExprVisitor {
        private:
        Polynomial                result_;
        std::string               input_;
        std::optional<Diagnostic> error_;

        public:
        /// Constructs the converter, optionally with the raw source string for
        /// diagnostic span labelling.
        ///
        /// # Arguments
        ///
        /// * `input` — Original source text; forwarded to error constructors.
        explicit ASTToPolynomial(const std::string& input = "")
            : input_(input) {}

        /// Converts `expr` to a `Polynomial`.
        ///
        /// Resets internal state before each call, so `convert` may be called
        /// multiple times on the same converter instance.
        ///
        /// # Arguments
        ///
        /// * `expr` — Root of the math expression AST to lower.
        ///
        /// # Returns
        ///
        /// The resulting `Polynomial` on success.
        ///
        /// # Errors
        ///
        /// Returns a `Diagnostic` (polynomial error) when the expression
        /// contains any of:
        /// - An `ArrayExpr` node.
        /// - A function call (`FunctionCall` node).
        /// - Division by a variable expression.
        /// - Division by zero.
        /// - An exponent that is not a non-negative integer constant.
        Result<Polynomial> convert(const Expr& expr) {
            error_.reset();
            expr.accept(*this);
            if (error_)
                return Result<Polynomial>::err(*error_);
            return Result<Polynomial>::ok(result_);
        }

        void visit(const ArrayExpr& node) override {
            error_ = errors::polynomial(
                "array value cannot appear in a polynomial expression",
                node.span(), input_);
        }

        void visit(const FunctionCall& node) override {
            error_ = errors::polynomial(
                "function call '" + node.name() +
                    "' cannot appear in polynomial expression",
                node.span(), input_);
        }

        void visit(const Number& node) override {
            result_ = Polynomial(node.value());
        }

        void visit(const Variable& node) override {
            result_ = Polynomial(1.0, node.name(), 1);
        }

        void visit(const UnaryOp& node) override {
            if (error_)
                return;
            node.operand().accept(*this);
            if (error_)
                return;
            switch (node.op()) {
            case UnaryOpType::Neg:
                result_ = result_ * Polynomial(-1.0);
                break;
            }
        }

        void visit(const BinaryOp& node) override {
            if (error_)
                return;
            // Lower left operand and store result
            node.left().accept(*this);
            Polynomial left = result_;

            if (error_)
                return;

            // Lower right operand and store result
            node.right().accept(*this);
            Polynomial right = result_;

            if (error_)
                return;

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
                    error_ = errors::polynomial(
                        "cannot divide by a variable expression",
                        node.right().span(), input_);
                    return;
                }
                // Eagerly check for division by zero to avoid undefined
                // behavior.
                if (std::abs(right.constant_value()) < kEpsilon) {
                    error_ = errors::polynomial("division by zero",
                                                node.right().span(), input_);
                    return;
                }
                result_ = left / right.constant_value();
                break;

            case BinaryOpType::Pow: {
                // Only allow exponentiation by non-negative integer constants.
                if (!right.is_constant()) {
                    error_ = errors::polynomial(
                        "exponent must be a non-negative integer constant",
                        node.right().span(), input_);
                    return;
                }
                double exp_val = right.constant_value();
                int    exp_int = static_cast<int>(std::round(exp_val));
                // Reject fractional or negative exponents; only allow integer
                // >= 0.
                if (std::abs(exp_val - exp_int) > kEpsilon || exp_int < 0) {
                    error_ = errors::polynomial(
                        "exponent must be a non-negative integer",
                        node.right().span(), input_);
                    return;
                }
                result_ = left.pow(exp_int);
                break;
            }
            }
        }
    };

} // namespace math_solver
