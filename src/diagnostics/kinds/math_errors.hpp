#pragma once

#include "diagnostics/diagnostic.hpp"
#include <string>

namespace math_solver {

    namespace errors {

        inline Diagnostic math(const std::string& message,
                               const Span&        span  = Span(),
                               const std::string& input = "",
                               const std::string& file  = "",
                               size_t             line  = 0) {
            return Diagnostic::make(message, "E0000", span, input)
                .with_location(file, line);
        }

        inline Diagnostic parse(const std::string& message,
                                const Span&        span  = Span(),
                                const std::string& input = "",
                                const std::string& file  = "",
                                size_t             line  = 0) {
            return Diagnostic::make(message, "E0001", span, input,
                                    "unexpected syntax")
                .with_location(file, line);
        }

        inline Diagnostic func_domain(const std::string& message,
                                      const Span&        span  = Span(),
                                      const std::string& input = "",
                                      const std::string& file  = "",
                                      size_t             line  = 0) {
            return Diagnostic::make(message, "E0002", span, input,
                                    "domain error")
                .with_location(file, line);
        }

    } // namespace errors

} // namespace math_solver
