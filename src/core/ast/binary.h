#ifndef BINARY_H
#define BINARY_H

#include "expr.h"
#include <memory>
#include <stdexcept>
#include <string>

namespace math_solver {

    enum class BinaryOpType { Add, Sub, Mul, Div, Pow };

    class BinaryOp : public Expr {
        private:
        ExprPtr      left_;
        ExprPtr      right_;
        BinaryOpType op_;

        public:
        BinaryOp(ExprPtr left, ExprPtr right, BinaryOpType op)
            : left_(std::move(left)), right_(std::move(right)), op_(op) {}

        const Expr&  left() const { return *left_; }
        const Expr&  right() const { return *right_; }
        BinaryOpType op() const { return op_; }

        void         accept(ExprVisitor& visitor) const override {
            visitor.visit(*this);
        }

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

        std::unique_ptr<Expr> clone() const override {
            return std::make_unique<BinaryOp>(left_->clone(), right_->clone(),
                                              op_);
        }
    };

} // namespace math_solver

#endif
