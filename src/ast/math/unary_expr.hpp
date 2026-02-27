#pragma once

#include "ast/math/expr.hpp"
#include <memory>
#include <string>

namespace math_solver {

    // Represents the set of supported unary prefix operations.
    enum class UnaryOpType { Neg };

    class UnaryOp : public Expr {
        private:
        ExprPtr     operand_;
        UnaryOpType op_;

        public:
        // Constructs a UnaryOp node, inferring the span from the operand.
        // The span does NOT cover the operator token; prefer the span-explicit
        // constructor when the operator source position is available.
        UnaryOp(ExprPtr operand, UnaryOpType op)
            : Expr(), operand_(std::move(operand)), op_(op) {
            if (operand_)
                span_ = operand_->span();
        }

        // Constructs a UnaryOp node with an explicit span.
        // span should cover from the operator token to the end of the operand.
        UnaryOp(ExprPtr operand, UnaryOpType op, const Span& span)
            : Expr(span), operand_(std::move(operand)), op_(op) {}

        const Expr& operand() const { return *operand_; }
        UnaryOpType op() const { return op_; }

        void        accept(ExprVisitor& visitor) const override {
            visitor.visit(*this);
        }

        std::string to_string() const override {
            switch (op_) {
            case UnaryOpType::Neg:
                return "(-" + operand_->to_string() + ")";
            }
            return "(??" + operand_->to_string() + ")";
        }

        // Deep clone; span is explicitly preserved to maintain source mapping.
        std::unique_ptr<Expr> clone() const override {
            auto cloned = std::make_unique<UnaryOp>(operand_->clone(), op_);
            cloned->set_span(span_);
            return cloned;
        }
    };

} // namespace math_solver
