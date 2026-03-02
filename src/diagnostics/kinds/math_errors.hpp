#pragma once

#include "diagnostics/diagnostic.hpp"

namespace math_solver {

    namespace errors {

        inline Diagnostic math(const std::string& message,
                               const Span&        span  = Span(),
                               const std::string& input = "") {
            return Diagnostic::make(message, "E0000", span, input);
        }

        inline Diagnostic parse(const std::string& message,
                                const Span&        span  = Span(),
                                const std::string& input = "") {
            return Diagnostic::make(message, "E0001", span, input,
                                    "unexpected syntax");
        }

        inline Diagnostic func_domain(const std::string& message,
                                      const Span&        span  = Span(),
                                      const std::string& input = "") {
            return Diagnostic::make(message, "E0002", span, input,
                                    "domain error");
        }

    } // namespace errors

} // namespace math_solver
