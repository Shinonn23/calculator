#ifndef NUMBER_ARRAY_H
#define NUMBER_ARRAY_H

#include "common/format_utils.h"
#include "expr.h"
#include <string>
#include <vector>


namespace math_solver {

    // Represents an array of numeric values (e.g., multiple roots of an
    // equation)
    class NumberArray : public Expr {
        private:
        std::vector<double> values_;

        public:
        explicit NumberArray(const std::vector<double>& values)
            : Expr(), values_(values) {}

        NumberArray(const std::vector<double>& values, const Span& span)
            : Expr(span), values_(values) {}

        const std::vector<double>& values() const { return values_; }
        size_t                     size() const { return values_.size(); }
        double                     at(size_t i) const { return values_.at(i); }

        void                       accept(ExprVisitor& visitor) const override {
            visitor.visit(*this);
        }

        std::string to_string() const override {
            std::string result = "[";
            for (size_t i = 0; i < values_.size(); ++i) {
                if (i > 0)
                    result += ", ";
                result += format_double(values_[i]);
            }
            result += "]";
            return result;
        }

        std::unique_ptr<Expr> clone() const override {
            auto cloned = std::make_unique<NumberArray>(values_);
            cloned->set_span(span_);
            return cloned;
        }
    };

} // namespace math_solver

#endif
