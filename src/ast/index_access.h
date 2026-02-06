#ifndef INDEX_ACCESS_H
#define INDEX_ACCESS_H

#include "expr.h"
#include <memory>
#include <string>

namespace math_solver {

    // Represents array index access: expr[index]
    // e.g., roots[0], solutions[1]
    class IndexAccess : public Expr {
        private:
        ExprPtr
            target_;   // The expression being indexed (e.g., Variable("roots"))
        size_t index_; // The integer index

        public:
        IndexAccess(ExprPtr target, size_t index)
            : Expr(), target_(std::move(target)), index_(index) {}

        IndexAccess(ExprPtr target, size_t index, const Span& span)
            : Expr(span), target_(std::move(target)), index_(index) {}

        const Expr& target() const { return *target_; }
        size_t      index() const { return index_; }

        void        accept(ExprVisitor& visitor) const override {
            visitor.visit(*this);
        }

        std::string to_string() const override {
            return target_->to_string() + "[" + std::to_string(index_) + "]";
        }

        std::unique_ptr<Expr> clone() const override {
            auto cloned =
                std::make_unique<IndexAccess>(target_->clone(), index_);
            cloned->set_span(span_);
            return cloned;
        }
    };

} // namespace math_solver

#endif
