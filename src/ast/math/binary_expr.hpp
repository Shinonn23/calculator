#pragma once

#include "ast/math/expr.hpp"
#include <memory>
#include <string>

namespace math_solver {

    // Represents the set of supported binary operations.
    // The order is relied upon by parser and codegen; do not reorder without
    // auditing all uses.
    enum class BinaryOpType { Add, Sub, Mul, Div, Pow };

    class BinaryOp : public Expr {
        private:
        ExprPtr      left_;
        ExprPtr      right_;
        BinaryOpType op_;

        public:
        // Constructs a BinaryOp node, inferring the span from its children.
        // Assumes both left_ and right_ are non-null and their spans are valid.
        // If either child is null, span_ remains default-initialized;
        // downstream code must handle this.
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

        // Constructs a BinaryOp node with an explicit span.
        // Used by deserialization and certain lowering passes where span is
        // precomputed.
        BinaryOp(ExprPtr left, ExprPtr right, BinaryOpType op, const Span& span)
            : Expr(span),
              left_(std::move(left)),
              right_(std::move(right)),
              op_(op) {}

        const Expr&  left() const { return *left_; }
        const Expr&  right() const { return *right_; }
        BinaryOpType op() const { return op_; }

        // Accepts a visitor; part of the classic visitor pattern for AST
        // traversal. Visitor is expected to handle all BinaryOpType variants.
        void         accept(ExprVisitor& visitor) const override {
            visitor.visit(*this);
        }

        // Returns a string representation of the binary operation.
        // Used for debugging and pretty-printing; not guaranteed to round-trip.
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

        // Deep clone of the subtree rooted at this node.
        // Span is explicitly set to preserve source mapping; this is relied
        // upon by later passes.
        std::unique_ptr<Expr> clone() const override {
            auto cloned = std::make_unique<BinaryOp>(
                left_->clone(), right_->clone(), op_);
            cloned->set_span(span_);
            return cloned;
        }
    };

} // namespace math_solver
