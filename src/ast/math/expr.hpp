#ifndef EXPR_H
#define EXPR_H

#include "ast/math/expr_visitor.hpp"
#include "core/span.hpp"
#include <memory>
#include <string>

namespace math_solver {

    // Base class for all expression nodes in the AST.
    //
    // - Each Expr carries a Span for error reporting and diagnostics.
    // - Subclasses must implement accept(), to_string(), and clone().
    // - Exprs are always heap-allocated and owned via unique_ptr to avoid
    //   accidental aliasing and to simplify lifetime management.
    // - The interface is intentionally minimal; all semantic logic is
    //   delegated to visitors or external passes.
    //
    // Invariant: span_ must always be valid and correspond to the source
    // region for this node. Mutations to span_ are only allowed during
    // construction or AST rewriting passes.
    class Expr {
        protected:
        Span span_;

        public:
        Expr() : span_() {}
        explicit Expr(const Span& span) : span_(span) {}
        virtual ~Expr() = default;

        const Span&         span() const { return span_; }
        void                set_span(const Span& span) { span_ = span; }

        // Accepts a visitor for double-dispatch. All semantic passes
        // (type checking, evaluation, etc.) should use this entry point.
        virtual void        accept(ExprVisitor& visitor) const = 0;

        // Returns a stable, lossless string representation of the expression.
        // Used for diagnostics and debugging; not guaranteed to be parseable.
        virtual std::string to_string() const                  = 0;

        // Produces a deep copy of the expression subtree.
        // Required for AST rewriting and speculative transformations.
        virtual std::unique_ptr<Expr> clone() const            = 0;
    };

    // Alias for heap-allocated expressions. All AST ownership flows through
    // this.
    using ExprPtr = std::unique_ptr<Expr>;

} // namespace math_solver

#endif
