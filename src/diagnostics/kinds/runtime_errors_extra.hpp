#pragma once

#include "diagnostics/diagnostic.hpp"

#include <string>

namespace math_solver {

    namespace errors {

        inline Diagnostic circular_dependency(const std::string& var_name,
                                              const Span&        span  = Span(),
                                              const std::string& input = "",
                                              const std::string& file  = "",
                                              size_t             line  = 0) {
            auto d = Diagnostic::make(
                         "cyclic dependency detected for `" + var_name + "`",
                         "E0391", span, input, "recursive variable reference")
                         .with_location(file, line);
            d.help = "ensure that variables do not depend on themselves "
                     "directly or indirectly.";
            return d;
        }

    } // namespace errors

} // namespace math_solver
