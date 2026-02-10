#ifndef EVALUATOR_H
#define EVALUATOR_H

#include "../ast/expr.h"
#include "context.h"

namespace math_solver {

    class Evaluator : public ExprVisitor {
        private:
        double         result_;
        const Context* context_;
        std::string    input_; // For error formatting

        public:
        Evaluator() : result_(0.0), context_(nullptr), input_() {}

        explicit Evaluator(const Context* ctx)
            : result_(0.0), context_(ctx), input_() {}

        Evaluator(const Context* ctx, const std::string& input)
            : result_(0.0), context_(ctx), input_(input) {}

        void   set_input(const std::string& input) { input_ = input; }

        double evaluate(const Expr& expr) {
            expr.accept(*this);
            return result_;
        }

        void visit(const Number& node) override;
        void visit(const BinaryOp& node) override;
        void visit(const Variable& node) override;
    };

} // namespace math_solver

#endif
