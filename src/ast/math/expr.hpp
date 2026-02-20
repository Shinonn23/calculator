#ifndef EXPR_H
#define EXPR_H

#include "core/span.hpp"
#include "ast/math/expr_visitor.hpp"
#include <memory>
#include <string>

namespace math_solver {
    class Expr {
        protected:
        Span span_;

        public:
        Expr() : span_() {}
        explicit Expr(const Span& span) : span_(span) {}
        virtual ~Expr() = default;

        const Span&         span() const { return span_; }
        void                set_span(const Span& span) { span_ = span; }

        virtual void        accept(ExprVisitor& visitor) const = 0;
        virtual std::string to_string() const                  = 0;
        virtual std::unique_ptr<Expr> clone() const            = 0;
    };

    using ExprPtr = std::unique_ptr<Expr>;

} // namespace math_solver

#endif
