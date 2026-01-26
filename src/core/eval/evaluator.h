#ifndef EVALUATOR_H
#define EVALUATOR_H

#include "../ast/expr.h"

namespace math_solver {

    class Evaluator : public ExprVisitor {
        private:
        double result_;

        public:
        Evaluator() : result_(0.0) {}

        double evaluate(const Expr& expr) {
            expr.accept(*this);
            return result_;
        }

        void visit(const Number& node) override;
        void visit(const BinaryOp& node) override;
    };

} // namespace math_solver

#endif
