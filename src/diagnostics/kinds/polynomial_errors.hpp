#pragma once

#include "diagnostics/diagnostic.hpp"

namespace math_solver {

    namespace errors {

        inline Diagnostic polynomial(const std::string& message,
                                     const Span&        span  = Span(),
                                     const std::string& input = "",
                                     const std::string& file  = "",
                                     size_t             line  = 0) {
            return Diagnostic::make(message, "E0310", span, input,
                                    "polynomial error")
                .with_location(file, line);
        }

    } // namespace errors

} // namespace math_solver
