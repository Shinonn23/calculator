#pragma once

//! # Module — `src/ast/math/expr.hpp`
//!
//! Defines the `Expr` abstract base class and the `ExprPtr` ownership alias.
//! All math expression AST nodes inherit from `Expr`. Semantic passes access
//! the tree exclusively through the `ExprVisitor` double-dispatch interface
//! rather than through virtual methods on `Expr` itself.

#include "ast/math/expr_visitor.hpp"
#include "core/span.hpp"
#include <memory>
#include <string>

namespace math_solver {

    /// Abstract base class for all math expression AST nodes.
    ///
    /// Every node carries a `Span` recording its source location for
    /// diagnostics. All semantic logic is delegated to `ExprVisitor`
    /// implementations; `Expr` itself is intentionally minimal. Nodes are
    /// always heap-allocated and owned through `ExprPtr` to prevent aliasing
    /// and simplify lifetime management.
    ///
    /// Invariant: `span_` must be valid and correspond to the source region of
    /// this node. It may only be mutated during construction or an AST
    /// rewriting pass via `set_span`.
    class Expr {
        protected:
        Span span_;

        public:
        Expr() : span_() {}
        explicit Expr(const Span& span) : span_(span) {}
        virtual ~Expr() = default;

        /// Return the source span associated with this node.
        const Span&         span() const { return span_; }

        /// Overwrite the source span.
        ///
        /// Only valid during construction or an AST rewriting pass.
        ///
        /// # Arguments
        ///
        /// * `span` — The new source region for this node.
        void                set_span(const Span& span) { span_ = span; }

        /// Accept a visitor for double-dispatch traversal.
        ///
        /// All semantic passes (evaluation, algebra, diagnostics) must enter
        /// the AST through this method.
        ///
        /// # Arguments
        ///
        /// * `visitor` — The visiting pass; must implement `ExprVisitor`.
        virtual void        accept(ExprVisitor& visitor) const = 0;

        /// Return a stable, lossless string representation for diagnostics.
        ///
        /// The output is not guaranteed to be re-parseable by the math lexer.
        virtual std::string to_string() const                  = 0;

        /// Produce a deep copy of the expression subtree.
        ///
        /// Required by AST rewriting passes and any transformation that must
        /// duplicate a subtree while preserving source mapping.
        ///
        /// # Returns
        ///
        /// A freshly allocated `ExprPtr` owning an independent copy of this
        /// node and all its descendants.
        virtual std::unique_ptr<Expr> clone() const            = 0;
    };

    /// Ownership alias for heap-allocated `Expr` nodes.
    ///
    /// All AST ownership and transfer flows through this alias.
    using ExprPtr = std::unique_ptr<Expr>;

} // namespace math_solver
