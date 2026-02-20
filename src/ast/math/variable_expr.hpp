#ifndef VARIABLE_H
#define VARIABLE_H

#include "ast/math/expr.hpp"
#include <string>

namespace math_solver {

    // Represents a named variable in the AST.
    //
    // Invariants:
    // - `name_` must be a valid identifier as parsed; no normalization is
    // performed here.
    // - Variable identity is determined solely by `name_` (and optionally
    // `span_` for diagnostics).
    //
    // Notes:
    // - Variable resolution and scoping are handled in later passes; this node
    // is a syntactic placeholder.
    // - Cloning preserves both the name and source span for accurate error
    // reporting.
    // - `accept` dispatches to the visitor; assumes visitor implements correct
    // overload.
    class Variable : public Expr {
        private:
        std::string name_;

        public:
        explicit Variable(const std::string& name) : Expr(), name_(name) {}

        Variable(const std::string& name, const Span& span)
            : Expr(span), name_(name) {}

        const std::string& name() const { return name_; }

        void               accept(ExprVisitor& visitor) const override {
            visitor.visit(*this);
        }

        std::string           to_string() const override { return name_; }

        std::unique_ptr<Expr> clone() const override {
            // Cloning preserves both identifier and span; required for
            // transformations that duplicate AST nodes while maintaining source
            // mapping.
            return std::make_unique<Variable>(name_, span_);
        }
    };

} // namespace math_solver

#endif
