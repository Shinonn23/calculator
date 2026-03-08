#pragma once

#include "diagnostics/diagnostic.hpp"

namespace math_solver {

    namespace errors {

        inline Diagnostic undefined_variable(const std::string& var_name,
                                             const Span&        span  = Span(),
                                             const std::string& input = "",
                                             const std::string& file  = "",
                                             size_t             line  = 0) {
            return Diagnostic::make(
                       "cannot find value `" + var_name + "` in this scope",
                       "E0425", span, input, "not found in this scope")
                .with_location(file, line);
        }

    } // namespace errors

} // namespace math_solver
