#pragma once

#include "diagnostics/diagnostic.hpp"

namespace math_solver {

    namespace errors {

        inline Diagnostic unknown_command(const std::string& command,
                                          const Span&        span  = Span(),
                                          const std::string& input = "") {
            auto d = Diagnostic::make("unknown command: " + command, "E0002",
                                      span, input, "unrecognized command");
            d.help = "available commands: :help, :env, :config, :history, "
                     "etc.";
            return d;
        }

    } // namespace errors

} // namespace math_solver
