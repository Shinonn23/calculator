#ifndef VARIABLE_H
#define VARIABLE_H

#include "expr.h"
#include <string>

namespace math_solver {

    class Variable : public Expr {
        private:
        std::string name_;

        public:
        explicit Variable(const std::string& name) : Expr(), name_(name) {}

        Variable(const std::string& name, const Span& span)
            : Expr(span), name_(name) {}

        const std::string& name() const { return name_; }

        void               accept(ExprVisitor& visitor) const override {
            visitor.visit(*this);
        }

        std::string           to_string() const override { return name_; }

        std::unique_ptr<Expr> clone() const override {
            return std::make_unique<Variable>(name_, span_);
        }
    };

} // namespace math_solver

#endif
