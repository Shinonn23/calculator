#pragma once

//! # Module — `src/ast/math/number_expr.hpp`
//!
//! Defines `Number`, the leaf AST node for numeric literals. Part of the math
//! AST layer; produced by the math parser and consumed by the evaluator and
//! algebra passes.

#include "ast/math/expr.hpp"
#include <string>

namespace math_solver {

    /// Leaf AST node representing a numeric literal.
    ///
    /// Invariant: `value_` must be a finite `double`; NaN and infinity are
    /// rejected at parse time. `span_` tracks the original source location for
    /// diagnostics. The string representation trims trailing zeros and a
    /// trailing decimal point for canonicalization; golden tests rely on this
    /// behaviour.
    class Number : public Expr {
        private:
        double value_;

        public:
        /// Construct a `Number` with no associated source span.
        explicit Number(double value) : Expr(), value_(value) {}

        /// Construct a `Number` with an explicit source span.
        ///
        /// # Arguments
        ///
        /// * `value` — The numeric value of this literal.
        /// * `span`  — Source region in the original input.
        Number(double value, const Span& span) : Expr(span), value_(value) {}

        /// Return the numeric value of this literal.
        double value() const { return value_; }

        /// Dispatch to `ExprVisitor::visit(const Number&)`.
        void   accept(ExprVisitor& visitor) const override {
            visitor.visit(*this);
        }

        /// Return the canonical string form of this literal.
        ///
        /// Trailing zeros after the decimal point and a trailing `.` are
        /// removed so that `1.0` renders as `"1"` and `1.50` as `"1.5"`.
        std::string to_string() const override {
            // The output format is intentionally minimal to avoid spurious
            // diffs in golden tests and to match user expectations for numeric
            // literals.
            std::string str = std::to_string(value_);
            str.erase(str.find_last_not_of('0') + 1, std::string::npos);
            if (str.back() == '.')
                str.pop_back();
            return str;
        }

        /// Produce a deep copy of this node, preserving both value and span.
        ///
        /// # Returns
        ///
        /// A new `Number` with identical `value_` and `span_`.
        std::unique_ptr<Expr> clone() const override {
            // Required for AST rewrites and passes that duplicate subtrees.
            return std::make_unique<Number>(value_, span_);
        }
    };

} // namespace math_solver
