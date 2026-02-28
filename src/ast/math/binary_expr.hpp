#pragma once

//! # Module — `src/ast/math/binary_expr.hpp`
//!
//! Defines `BinaryOpType` and `BinaryOp`, the interior AST node for binary
//! arithmetic operations. Part of the math AST layer; produced by the
//! recursive-descent math parser and consumed by the evaluator, algebra, and
//! polynomial passes.

#include "ast/math/expr.hpp"
#include <memory>
#include <string>

namespace math_solver {

    /// Enumeration of all supported binary operators.
    ///
    /// The ordering is relied upon by parser and code-generation logic; do not
    /// reorder variants without auditing all downstream uses.
    enum class BinaryOpType { Add, Sub, Mul, Div, Pow };

    /// Interior AST node representing a binary arithmetic operation.
    ///
    /// Owns its left and right child subtrees via `ExprPtr`. The source span
    /// is computed as the merged span of both children unless explicitly
    /// supplied via the span-explicit constructor.
    class BinaryOp : public Expr {
        private:
        ExprPtr      left_;
        ExprPtr      right_;
        BinaryOpType op_;

        public:
        /// Construct a `BinaryOp`, inferring the span from the child nodes.
        ///
        /// The span is set to `left->span().merge(right->span())` when both
        /// children are non-null; otherwise it is default-initialized.
        ///
        /// # Arguments
        ///
        /// * `left`  — Left-hand child subtree; must be non-null for a valid span.
        /// * `right` — Right-hand child subtree; must be non-null for a valid span.
        /// * `op`    — The binary operator applied to both operands.
        BinaryOp(ExprPtr left, ExprPtr right, BinaryOpType op)
            : Expr(),
              left_(std::move(left)),
              right_(std::move(right)),
              op_(op) {
            if (left_ && right_) {
                // Span is conservatively merged from both children.
                // This is relied upon by diagnostics and source mapping.
                span_ = left_->span().merge(right_->span());
            }
        }

        /// Construct a `BinaryOp` with an explicit source span.
        ///
        /// Used by deserialization and lowering passes where the span is
        /// precomputed rather than derived from the children.
        ///
        /// # Arguments
        ///
        /// * `left`  — Left-hand child subtree.
        /// * `right` — Right-hand child subtree.
        /// * `op`    — The binary operator.
        /// * `span`  — Explicit source region covering this node.
        BinaryOp(ExprPtr left, ExprPtr right, BinaryOpType op, const Span& span)
            : Expr(span),
              left_(std::move(left)),
              right_(std::move(right)),
              op_(op) {}

        /// Return the left-hand child expression.
        const Expr&  left() const { return *left_; }

        /// Return the right-hand child expression.
        const Expr&  right() const { return *right_; }

        /// Return the binary operator kind.
        BinaryOpType op() const { return op_; }

        /// Dispatch to `ExprVisitor::visit(const BinaryOp&)`.
        void         accept(ExprVisitor& visitor) const override {
            visitor.visit(*this);
        }

        /// Return a parenthesised infix string representation.
        ///
        /// Used for diagnostics and pretty-printing; not guaranteed to
        /// round-trip through the math parser.
        ///
        /// # Examples
        ///
        /// ```cpp
        /// // A BinaryOp for 2 + 3 renders as "(2 + 3)".
        /// ```
        std::string to_string() const override {
            const char* op_str = "?";
            switch (op_) {
            case BinaryOpType::Add:
                op_str = "+";
                break;
            case BinaryOpType::Sub:
                op_str = "-";
                break;
            case BinaryOpType::Mul:
                op_str = "*";
                break;
            case BinaryOpType::Div:
                op_str = "/";
                break;
            case BinaryOpType::Pow:
                op_str = "^";
                break;
            }
            return "(" + left_->to_string() + " " + op_str + " " +
                   right_->to_string() + ")";
        }

        /// Produce a deep copy of the subtree rooted at this node.
        ///
        /// The span is explicitly set on the clone to preserve source mapping
        /// for downstream diagnostic passes.
        ///
        /// # Returns
        ///
        /// A new `BinaryOp` with independent copies of both child subtrees.
        std::unique_ptr<Expr> clone() const override {
            auto cloned = std::make_unique<BinaryOp>(
                left_->clone(), right_->clone(), op_);
            cloned->set_span(span_);
            return cloned;
        }
    };

} // namespace math_solver
