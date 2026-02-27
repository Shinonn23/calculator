#pragma once

namespace math_solver {
    class Number;
    class BinaryOp;
    class UnaryOp;
    class Variable;
    class Equation;

    // ExprVisitor provides a double-dispatch mechanism for traversing and
    // operating on the expression AST. Implementations must ensure that
    // visit methods do not mutate the AST nodes directly, as other passes
    // may rely on structural immutability. The interface is intentionally
    // minimal to avoid coupling to evaluation or transformation logic.
    //
    // Note: If new node types are added to the AST, this interface must be
    // updated accordingly to avoid silent omissions in downstream passes.
    class ExprVisitor {
        public:
        virtual ~ExprVisitor()                   = default;
        virtual void visit(const Number& node)   = 0;
        virtual void visit(const BinaryOp& node) = 0;
        virtual void visit(const UnaryOp& node)  = 0;
        virtual void visit(const Variable& node) = 0;
    };
} // namespace math_solver
