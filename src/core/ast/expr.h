#ifndef EXPR_H
#define EXPR_H

#include <memory>
#include <string>

namespace math_solver {

    class Number;
    class BinaryOp;

    class ExprVisitor {
        public:
        virtual void visit(const Number& node)   = 0;
        virtual void visit(const BinaryOp& node) = 0;
    };

    class Expr {
        public:
        virtual void                  accept(ExprVisitor& visitor) const = 0;
        virtual std::string           to_string() const                  = 0;
        virtual std::unique_ptr<Expr> clone() const                      = 0;
    };

    using ExprPtr = std::unique_ptr<Expr>;

} // namespace math_solver

#endif
