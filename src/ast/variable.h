#ifndef VARIABLE_H
#define VARIABLE_H

#include "expr.h"
#include <string>

using namespace std;

namespace math_solver {
    class Variable : public Expr {
        private:
        string name_;

        public:
        explicit Variable(const string& name) : Expr(), name_(name) {}

        Variable(const string& name, const Span& span)
            : Expr(span), name_(name) {}

        const string& name() const { return name_; }

        void accept(ExprVisitor& visitor) const override {
            visitor.visit(*this);
        }

        string to_string() const override {
            return name_;
        }

        unique_ptr<Expr> clone() const override {
            return make_unique<Variable>(name_, span_);
        }
    };

} // namespace math_solver

#endif
