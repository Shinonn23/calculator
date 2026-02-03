#ifndef NUMBER_H
#define NUMBER_H

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

        std::string to_string() const override {
            std::string str = std::to_string(value_);
            str.erase(str.find_last_not_of('0') + 1, std::string::npos);
            if (str.back() == '.')
                str.pop_back();
            return str;
        }

        std::unique_ptr<Expr> clone() const override {
            return std::make_unique<Number>(value_, span_);
        }
    };

} // namespace math_solver

#endif
