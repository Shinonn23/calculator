#ifndef EQUATION_H
#define EQUATION_H

#include "expr.h"
#include <memory>
#include <string>

namespace math_solver {

    // Represents an equation: lhs = rhs
    // Note: Equation is not an Expr (cannot be nested in expressions)
    class Equation {
        private:
        ExprPtr lhs_;
        ExprPtr rhs_;
        Span    span_;

        public:
        Equation(ExprPtr lhs, ExprPtr rhs)
            : lhs_(std::move(lhs)), rhs_(std::move(rhs)) {
            if (lhs_ && rhs_) {
                span_ = lhs_->span().merge(rhs_->span());
            }
        }

        Equation(ExprPtr lhs, ExprPtr rhs, const Span& span)
            : lhs_(std::move(lhs)), rhs_(std::move(rhs)), span_(span) {}

        const Expr& lhs() const { return *lhs_; }
        const Expr& rhs() const { return *rhs_; }
        const Span& span() const { return span_; }

        // Move ownership out
        ExprPtr     take_lhs() { return std::move(lhs_); }
        ExprPtr     take_rhs() { return std::move(rhs_); }

        std::string to_string() const {
            return lhs_->to_string() + " = " + rhs_->to_string();
        }

        std::unique_ptr<Equation> clone() const {
            return std::make_unique<Equation>(lhs_->clone(), rhs_->clone(),
                                              span_);
        }
    };

    using EquationPtr = std::unique_ptr<Equation>;

} // namespace math_solver

#endif
