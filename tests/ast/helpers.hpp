#pragma once

#include "ast/math/number_expr.hpp"
#include "ast/math/variable_expr.hpp"

#include <memory>
#include <string>

namespace test_helpers {

using namespace math_solver;

inline ExprPtr make_num(double v, size_t s = 0, size_t e = 0) {
    return std::make_unique<Number>(v, Span(s, e));
}

inline ExprPtr make_var(const std::string& name, size_t s = 0, size_t e = 0) {
    return std::make_unique<Variable>(name, Span(s, e));
}

} // namespace test_helpers
