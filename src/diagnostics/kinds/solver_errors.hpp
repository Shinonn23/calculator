#pragma once

#include "diagnostics/diagnostic.hpp"

#include <string>
#include <vector>

namespace math_solver {

    namespace errors {

        inline Diagnostic unsupported_equation(const std::string& message,
                                              const Span&        span  = Span(),
                                              const std::string& input = "") {
            return Diagnostic::make(message, "E0315", span, input,
                                    "unsupported");
        }

        inline Diagnostic no_solution(const std::string& message,
                                      const Span&        span  = Span(),
                                      const std::string& input = "") {
            return Diagnostic::make(message, "E0301", span, input,
                                    "no solution");
        }

        inline Diagnostic infinite_solutions(const std::string& message,
                                             const Span&        span  = Span(),
                                             const std::string& input = "") {
            return Diagnostic::make(message, "E0302", span, input,
                                    "infinite solutions");
        }

        inline Diagnostic invalid_equation(const std::string& message,
                                           const Span&        span  = Span(),
                                           const std::string& input = "") {
            return Diagnostic::make(message, "E0303", span, input,
                                    "invalid equation");
        }

        inline Diagnostic
        multiple_unknowns(const std::vector<std::string>& vars,
                          const Span&                     span  = Span(),
                          const std::string&              input = "") {
            std::string msg = "equation has multiple unknowns";
            if (!vars.empty()) {
                msg += ": ";
                for (size_t i = 0; i < vars.size(); ++i) {
                    if (i)
                        msg += ", ";
                    msg += vars[i];
                }
            }
            auto d = Diagnostic::make(msg, "E0304", span, input,
                                      "multiple unknowns");
            d.help = "provide values for other variables using :set";
            return d;
        }

        inline Diagnostic system_no_solution(const std::string& message,
                                             const Span&        span  = Span(),
                                             const std::string& input = "") {
            return Diagnostic::make(message, "E0310", span, input,
                                    "no solution");
        }

        inline Diagnostic
        system_infinite_solutions(const std::string& message,
                                  const Span&        span  = Span(),
                                  const std::string& input = "") {
            auto d = Diagnostic::make(message, "E0311", span, input,
                                      "infinite solutions");
            d.help = "use --free-vars to see parameterised form";
            return d;
        }

    } // namespace errors

} // namespace math_solver
