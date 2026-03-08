#pragma once

//! # Module — `src/ast/math/array_expr.hpp`
//!
//! Defines `ArrayExpr`, a compound AST node representing an ordered list of
//! expressions. Used by the solver to store multi-root results and by the
//! evaluator to perform broadcast (vectorised) evaluation.

#include "ast/math/expr.hpp"
#include "ast/math/expr_visitor.hpp"

#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace math_solver {

    /// AST node representing an array (ordered list) of expressions.
    ///
    /// Invariants:
    /// - `elements_` may be empty (denotes the empty array `[]`).
    /// - Each element is a valid, non-null `ExprPtr`.
    /// - `to_string()` produces `[e0, e1, ..., en]` using each element's
    ///   canonical representation.
    ///
    /// Usage: produced by the solver when a polynomial equation has multiple
    /// roots, and consumed by the evaluator's broadcast path when the variable
    /// is referenced in a subsequent expression.
    class ArrayExpr : public Expr {
        private:
        std::vector<ExprPtr> elements_;

        public:
        /// Construct an `ArrayExpr` taking ownership of `elements`, with no source span.
        ///
        /// # Arguments
        ///
        /// * `elements` — The ordered list of child expression nodes; may be empty.
        explicit ArrayExpr(std::vector<ExprPtr> elements)
            : Expr(), elements_(std::move(elements)) {}

        /// Construct an `ArrayExpr` with an explicit source span.
        ///
        /// # Arguments
        ///
        /// * `elements` — The ordered list of child expression nodes; may be empty.
        /// * `span`     — Source region in the original input covering this array.
        ArrayExpr(std::vector<ExprPtr> elements, const Span& span)
            : Expr(span), elements_(std::move(elements)) {}

        /// Returns a const reference to the element list.
        const std::vector<ExprPtr>& elements() const { return elements_; }

        /// Returns the number of elements.
        size_t                      size() const { return elements_.size(); }

        /// Returns true if there are no elements.
        bool                        empty() const { return elements_.empty(); }

        /// Dispatch to `ExprVisitor::visit(const ArrayExpr&)`.
        void accept(ExprVisitor& visitor) const override {
            visitor.visit(*this);
        }

        /// Returns `[e0, e1, ..., en]` using each element's `to_string()`.
        std::string to_string() const override {
            std::ostringstream oss;
            oss << "[";
            for (size_t i = 0; i < elements_.size(); ++i) {
                if (i)
                    oss << ", ";
                oss << elements_[i]->to_string();
            }
            oss << "]";
            return oss.str();
        }

        /// Produce a deep copy of this node and all element subtrees.
        ///
        /// # Returns
        ///
        /// A new `ArrayExpr` with independent copies of every element and the
        /// same `span_`.
        std::unique_ptr<Expr> clone() const override {
            std::vector<ExprPtr> cloned;
            cloned.reserve(elements_.size());
            for (const auto& e : elements_)
                cloned.push_back(e->clone());
            return std::make_unique<ArrayExpr>(std::move(cloned), span_);
        }
    };

} // namespace math_solver
