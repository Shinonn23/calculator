#pragma once

//! # Module — `src/ast/math/unary_expr.hpp`
//!
//! Defines `UnaryOpType` and `UnaryOp`, the interior AST node for unary prefix
//! operations. Currently only negation (`Neg`) is supported. Part of the math
//! AST layer; produced by the math parser during prefix-operator handling.

#include "ast/math/expr.hpp"
#include <memory>
#include <string>

namespace math_solver {

    /// Enumeration of all supported unary prefix operators.
    enum class UnaryOpType { Neg };

    /// Interior AST node representing a unary prefix operation.
    ///
    /// Owns its operand subtree via `ExprPtr`. When constructed without an
    /// explicit span, the span is inherited from the operand and therefore
    /// does not cover the operator token itself; use the span-explicit
    /// constructor when accurate source mapping of the operator position is
    /// required.
    class UnaryOp : public Expr {
        private:
        ExprPtr     operand_;
        UnaryOpType op_;

        public:
        /// Construct a `UnaryOp`, inferring the span from the operand.
        ///
        /// The resulting span covers only the operand, not the leading operator
        /// token. Prefer the span-explicit overload when the operator source
        /// position is available.
        ///
        /// # Arguments
        ///
        /// * `operand` — The operand subtree; must be non-null for a valid span.
        /// * `op`      — The unary operator kind.
        UnaryOp(ExprPtr operand, UnaryOpType op)
            : Expr(), operand_(std::move(operand)), op_(op) {
            if (operand_)
                span_ = operand_->span();
        }

        /// Construct a `UnaryOp` with an explicit source span.
        ///
        /// The span should cover from the operator token to the end of the
        /// operand.
        ///
        /// # Arguments
        ///
        /// * `operand` — The operand subtree.
        /// * `op`      — The unary operator kind.
        /// * `span`    — Explicit source region including the operator token.
        UnaryOp(ExprPtr operand, UnaryOpType op, const Span& span)
            : Expr(span), operand_(std::move(operand)), op_(op) {}

        /// Return the operand child expression.
        const Expr& operand() const { return *operand_; }

        /// Return the unary operator kind.
        UnaryOpType op() const { return op_; }

        /// Dispatch to `ExprVisitor::visit(const UnaryOp&)`.
        void        accept(ExprVisitor& visitor) const override {
            visitor.visit(*this);
        }

        /// Return a parenthesised string representation of the unary operation.
        ///
        /// `Neg` renders as `"(-<operand>)"`. An unrecognised operator renders
        /// as `"(??<operand>)"` as a diagnostic fallback.
        std::string to_string() const override {
            switch (op_) {
            case UnaryOpType::Neg:
                return "(-" + operand_->to_string() + ")";
            }
            return "(??" + operand_->to_string() + ")";
        }

        /// Produce a deep copy of the subtree rooted at this node.
        ///
        /// The span is explicitly set on the clone to preserve source mapping.
        ///
        /// # Returns
        ///
        /// A new `UnaryOp` with an independent copy of the operand subtree.
        std::unique_ptr<Expr> clone() const override {
            auto cloned = std::make_unique<UnaryOp>(operand_->clone(), op_);
            cloned->set_span(span_);
            return cloned;
        }
    };

} // namespace math_solver
