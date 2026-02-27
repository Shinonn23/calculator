#pragma once

#include "diagnostics/diagnostic.hpp"

#include <string>

namespace math_solver {

    namespace errors {

        inline Diagnostic circular_dependency(const std::string& var_name,
                                              const Span&        span  = Span(),
                                              const std::string& input = "") {
            auto d = Diagnostic::make(
                "cyclic dependency detected for `" + var_name + "`", "E0391",
                span, input, "recursive variable reference");
            d.help = "ensure that variables do not depend on themselves "
                     "directly or indirectly.";
            return d;
        }

    } // namespace errors

} // namespace math_solver
