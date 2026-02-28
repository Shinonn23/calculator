#pragma once

//! # Module — `src/ast/math/equation_expr.hpp`
//!
//! Defines `Equation`, the top-level AST node for equality constraints, and
//! its ownership alias `EquationPtr`. `Equation` is intentionally not a
//! subtype of `Expr` so that equations cannot be accidentally nested inside
//! expressions, which would violate solver invariants.

#include "ast/math/expr.hpp"
#include <memory>
#include <string>

namespace math_solver {

    /// Top-level AST node for an equality constraint between two expressions.
    ///
    /// `Equation` is not an `Expr` subclass; this prevents nesting equations
    /// inside expressions, which the solver does not support. Both `lhs_` and
    /// `rhs_` are always non-null after construction. The source span is
    /// computed as the merged span of both sides unless explicitly provided.
    ///
    /// Ownership transfers out of `lhs_` or `rhs_` via `take_lhs()` /
    /// `take_rhs()`, leaving the respective pointer null. Callers must not
    /// access `lhs()` or `rhs()` after the corresponding `take_*()` call.
    class Equation {
        private:
        ExprPtr lhs_;
        ExprPtr rhs_;
        Span    span_;

        public:
        /// Construct an `Equation`, inferring the span from both sides.
        ///
        /// The span is set to `lhs->span().merge(rhs->span())` when both sides
        /// are non-null.
        ///
        /// # Arguments
        ///
        /// * `lhs` — Left-hand side expression; must be non-null.
        /// * `rhs` — Right-hand side expression; must be non-null.
        Equation(ExprPtr lhs, ExprPtr rhs)
            : lhs_(std::move(lhs)), rhs_(std::move(rhs)) {
            if (lhs_ && rhs_) {
                span_ = lhs_->span().merge(rhs_->span());
            }
        }

        /// Construct an `Equation` with an explicit source span.
        ///
        /// # Arguments
        ///
        /// * `lhs`  — Left-hand side expression.
        /// * `rhs`  — Right-hand side expression.
        /// * `span` — Explicit source region covering the full equation.
        Equation(ExprPtr lhs, ExprPtr rhs, const Span& span)
            : lhs_(std::move(lhs)), rhs_(std::move(rhs)), span_(span) {}

        /// Return the left-hand side expression.
        ///
        /// # Panics
        ///
        /// Undefined behaviour if `take_lhs()` has already been called.
        const Expr& lhs() const { return *lhs_; }

        /// Return the right-hand side expression.
        ///
        /// # Panics
        ///
        /// Undefined behaviour if `take_rhs()` has already been called.
        const Expr& rhs() const { return *rhs_; }

        /// Return the source span of the full equation.
        const Span& span() const { return span_; }

        /// Transfer ownership of the left-hand side out of this node.
        ///
        /// After this call `lhs_` is null; `lhs()` must not be called again.
        ExprPtr     take_lhs() { return std::move(lhs_); }

        /// Transfer ownership of the right-hand side out of this node.
        ///
        /// After this call `rhs_` is null; `rhs()` must not be called again.
        ExprPtr     take_rhs() { return std::move(rhs_); }

        /// Return an infix string representation of the equation.
        std::string to_string() const {
            return lhs_->to_string() + " = " + rhs_->to_string();
        }

        /// Produce a deep copy of this equation and both of its subtrees.
        ///
        /// # Returns
        ///
        /// A new `Equation` with independent copies of `lhs_`, `rhs_`, and
        /// the same `span_`.
        std::unique_ptr<Equation> clone() const {
            return std::make_unique<Equation>(
                lhs_->clone(), rhs_->clone(), span_);
        }
    };

    /// Ownership alias for heap-allocated `Equation` nodes.
    using EquationPtr = std::unique_ptr<Equation>;

} // namespace math_solver

