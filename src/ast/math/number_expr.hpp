#ifndef NUMBER_H
#define NUMBER_H

#include "ast/math/expr.hpp"
#include <string>

namespace math_solver {

    // Represents a numeric literal in the AST.
    //
    // Invariant: `value_` is always a finite double (NaN/Inf should be rejected
    // at parse time). The `span_` field tracks the original source location for
    // diagnostics.
    //
    // Note: The string conversion logic trims trailing zeros and the decimal
    // point for canonicalization, which is relied upon by pretty-printers and
    // test output.
    //
    // Cloning preserves both value and span, which is required for
    // transformations that need to maintain source mapping (e.g., error
    // reporting, macro expansion).
    class Number : public Expr {
        private:
        double value_;

        public:
        explicit Number(double value) : Expr(), value_(value) {}

        Number(double value, const Span& span) : Expr(span), value_(value) {}

        double value() const { return value_; }

        void   accept(ExprVisitor& visitor) const override {
            visitor.visit(*this);
        }

        std::string to_string() const override {
            // The output format is intentionally minimal to avoid spurious
            // diffs in golden tests and to match user expectations for numeric
            // literals.
            std::string str = std::to_string(value_);
            str.erase(str.find_last_not_of('0') + 1, std::string::npos);
            if (str.back() == '.')
                str.pop_back();
            return str;
        }

        std::unique_ptr<Expr> clone() const override {
            // Required for AST rewrites and passes that duplicate subtrees.
            return std::make_unique<Number>(value_, span_);
        }
    };

} // namespace math_solver

#endif
