#ifndef EXPR_VISITOR_H
#define EXPR_VISITOR_H

namespace math_solver {
    class Number;
    class BinaryOp;
    class Variable;
    class Equation;
    
    class ExprVisitor {
        public:
        virtual ~ExprVisitor()                   = default;
        virtual void visit(const Number& node)   = 0;
        virtual void visit(const BinaryOp& node) = 0;
        virtual void visit(const Variable& node) = 0;
        virtual void visit(const Equation& node) = 0;
    };
} // namespace math_solver

#endif