#ifndef NUMBER_H
#define NUMBER_H

#include "common/format_utils.h"
#include "expr.h"
#include <string>


namespace math_solver {

    class Number : public Expr {
        private:
        double value_;

        public:
        explicit Number(double value) : Expr(), value_(value) {}

        Number(double value, const Span& span) : Expr(span), value_(value) {}

        double value() const { return value_; }

        void   accept(ExprVisitor& visitor) const override {
            visitor.visit(*this);
        }

        std::string to_string() const override { return format_double(value_); }

        std::unique_ptr<Expr> clone() const override {
            return std::make_unique<Number>(value_, span_);
        }
    };

} // namespace math_solver

#endif
