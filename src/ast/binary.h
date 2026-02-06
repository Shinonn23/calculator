#ifndef BINARY_H
#define BINARY_H

#include "expr.h"
#include <memory>
#include <stdexcept>
#include <string>

using namespace std;

namespace math_solver {
    enum class BinaryOpType { Add, Sub, Mul, Div, Pow };

    class BinaryOp : public Expr {
        private:
        ExprPtr      left_;
        ExprPtr      right_;
        BinaryOpType op_;

        public:
        BinaryOp(ExprPtr left, ExprPtr right, BinaryOpType op)
            : Expr(), left_(move(left)), right_(move(right)), op_(op) {
            if (left_ && right_) {
                span_ = left_->span().merge(right_->span());
            }
        }

        BinaryOp(ExprPtr left, ExprPtr right, BinaryOpType op, const Span& span)
            : Expr(span), left_(move(left)), right_(move(right)), op_(op) {}

        const Expr&  left() const { return *left_; }
        const Expr&  right() const { return *right_; }
        BinaryOpType op() const { return op_; }

        void         accept(ExprVisitor& visitor) const override {
            visitor.visit(*this);
        }

        string to_string() const override {
            const char* op_str = "?";
            switch (op_) {
            case BinaryOpType::Add:
                op_str = " + ";
                break;
            case BinaryOpType::Sub:
                op_str = " - ";
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

            string left_str   = left_->to_string();
            string right_str  = right_->to_string();

            // Add parentheses only when necessary for precedence
            auto   precedence = [](BinaryOpType t) -> int {
                switch (t) {
                case BinaryOpType::Add:
                case BinaryOpType::Sub:
                    return 1;
                case BinaryOpType::Mul:
                case BinaryOpType::Div:
                    return 2;
                case BinaryOpType::Pow:
                    return 3;
                }
                return 0;
            };

            int   my_prec  = precedence(op_);

            // Wrap left child if it has lower precedence
            auto* left_bin = dynamic_cast<const BinaryOp*>(left_.get());
            if (left_bin && precedence(left_bin->op()) < my_prec) {
                left_str = "(" + left_str + ")";
            }

            // Wrap right child if it has lower precedence,
            // or same precedence for Sub/Div (left-associative)
            auto* right_bin = dynamic_cast<const BinaryOp*>(right_.get());
            if (right_bin) {
                int rp = precedence(right_bin->op());
                if (rp < my_prec ||
                    (rp == my_prec &&
                     (op_ == BinaryOpType::Sub || op_ == BinaryOpType::Div))) {
                    right_str = "(" + right_str + ")";
                }
            }

            return left_str + op_str + right_str;
        }

        unique_ptr<Expr> clone() const override {
            auto cloned =
                make_unique<BinaryOp>(left_->clone(), right_->clone(), op_);
            cloned->set_span(span_);
            return cloned;
        }
    };

} // namespace math_solver

#endif
