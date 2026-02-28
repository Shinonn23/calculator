#pragma once

//! # Module — `src/ast/math/variable_expr.hpp`
//!
//! Defines `Variable`, the leaf AST node for named identifiers. Part of the
//! math AST layer; produced by the math parser and resolved later by the
//! runtime resolver pass (`src/runtime/context/resolver.cpp`).

#include "ast/math/expr.hpp"
#include <string>

namespace math_solver {

    /// Leaf AST node representing a named variable reference.
    ///
    /// `Variable` is a syntactic placeholder; resolution and scoping are
    /// handled by later passes. Identity is determined by `name_` alone; no
    /// normalization of the identifier string is performed here.
    class Variable : public Expr {
        private:
        std::string name_;

        public:
        /// Construct a `Variable` with no associated source span.
        explicit Variable(const std::string& name) : Expr(), name_(name) {}

        /// Construct a `Variable` with an explicit source span.
        ///
        /// # Arguments
        ///
        /// * `name` — The raw identifier string as it appears in the source.
        /// * `span` — Source region in the original input.
        Variable(const std::string& name, const Span& span)
            : Expr(span), name_(name) {}

        /// Return the identifier string for this variable.
        const std::string& name() const { return name_; }

        /// Dispatch to `ExprVisitor::visit(const Variable&)`.
        void               accept(ExprVisitor& visitor) const override {
            visitor.visit(*this);
        }

        /// Return the identifier string as the string representation.
        std::string           to_string() const override { return name_; }

        /// Produce a deep copy of this node, preserving both name and span.
        ///
        /// # Returns
        ///
        /// A new `Variable` with identical `name_` and `span_`.
        std::unique_ptr<Expr> clone() const override {
            // Cloning preserves both identifier and span; required for
            // transformations that duplicate AST nodes while maintaining source
            // mapping.
            return std::make_unique<Variable>(name_, span_);
        }
    };

} // namespace math_solver
