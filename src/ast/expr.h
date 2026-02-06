#ifndef EXPR_H
#define EXPR_H

#include "common/span.h"
#include <memory>
#include <string>

namespace math_solver {

    class Number;
    class BinaryOp;
    class Variable;
    class Equation;

    class ExprVisitor {
        public:
        virtual ~ExprVisitor() = default;
        virtual void visit(const Number& node)   = 0;
        virtual void visit(const BinaryOp& node) = 0;
        virtual void visit(const Variable& node) = 0;
    };

    class Expr {
        protected:
        Span span_;

        public:
        Expr() : span_() {}
        explicit Expr(const Span& span) : span_(span) {}
        virtual ~Expr() = default;

        const Span& span() const { return span_; }
        void set_span(const Span& span) { span_ = span; }

        virtual void                  accept(ExprVisitor& visitor) const = 0;
        virtual std::string           to_string() const                  = 0;
        virtual std::unique_ptr<Expr> clone() const                      = 0;
    };

    using ExprPtr = std::unique_ptr<Expr>;

} // namespace math_solver

#endif
