#pragma once

//! # Module — `src/ast/math/expr_visitor.hpp`
//!
//! Defines the `ExprVisitor` interface used by all math AST traversal passes.
//! Part of the math AST layer; evaluation, algebra, and pretty-printing passes
//! implement this interface to walk the expression tree via double-dispatch.
namespace math_solver {
    class Number;
    class BinaryOp;
    class UnaryOp;
    class Variable;
    class Equation;
    class ArrayExpr;

    /// Pure abstract visitor for the math expression AST.
    ///
    /// Provides a closed set of `visit` overloads, one per concrete node type.
    /// All passes that traverse the math AST must implement every overload; the
    /// absence of defaults forces explicit handling and prevents silent dispatch
    /// failures. When a new node type is added to the AST, a corresponding
    /// `visit` overload must be added here in lockstep.
    class ExprVisitor {
        public:
        virtual ~ExprVisitor()                      = default;

        /// Dispatch to the visitor implementation for a `Number` leaf node.
        virtual void visit(const Number& node)      = 0;

        /// Dispatch to the visitor implementation for a `BinaryOp` interior node.
        virtual void visit(const BinaryOp& node)    = 0;

        /// Dispatch to the visitor implementation for a `UnaryOp` interior node.
        virtual void visit(const UnaryOp& node)     = 0;

        /// Dispatch to the visitor implementation for a `Variable` leaf node.
        virtual void visit(const Variable& node)    = 0;

        /// Dispatch to the visitor implementation for an `ArrayExpr` node.
        virtual void visit(const ArrayExpr& node)   = 0;
    };
} // namespace math_solver
