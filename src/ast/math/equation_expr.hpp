#ifndef EQUATION_H
#define EQUATION_H

#include "ast/math/expr.hpp"
#include <memory>
#include <string>

namespace math_solver {

    // Equation represents a top-level equality constraint between two Exprs.
    //
    // Invariant: lhs_ and rhs_ are always non-null after construction.
    // Equation is intentionally *not* an Expr; this prevents accidental
    // nesting of equations within expressions, which would break solver
    // assumptions elsewhere in the pipeline.
    //
    // The span_ field is computed as the merged span of lhs_ and rhs_ unless
    // explicitly provided. This is relied upon by diagnostics and error
    // reporting to accurately reflect the source range of the equation.
    //
    // Ownership: Equation owns its lhs_ and rhs_ expressions. take_lhs() and
    // take_rhs() transfer ownership out, leaving the respective pointer null.
    // Callers must not use lhs() or rhs() after take_*() has been called.
    //
    // Cloning: clone() performs a deep copy of the equation and its subtrees.
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

        ExprPtr     take_lhs() { return std::move(lhs_); }
        ExprPtr     take_rhs() { return std::move(rhs_); }

        std::string to_string() const {
            return lhs_->to_string() + " = " + rhs_->to_string();
        }

        std::unique_ptr<Equation> clone() const {
            return std::make_unique<Equation>(
                lhs_->clone(), rhs_->clone(), span_);
        }
    };

    using EquationPtr = std::unique_ptr<Equation>;

} // namespace math_solver

#endif
