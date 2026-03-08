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

        // Emitted when a flag-like token (e.g. --no-save) appears at the end
        // of an unquoted expression payload and would be silently ignored.
        // Instructs the user to quote the expression so trailing flags are
        // parsed correctly.
        inline Diagnostic trailing_flag_after_expr(const std::string& flag,
                                                   const std::string& input) {
            // Point the span at the flag itself, not the start of the line.
            size_t pos  = input.rfind(flag);
            Span   span = (pos != std::string::npos)
                              ? Span(pos, pos + flag.size())
                              : Span{};
            auto d = Diagnostic::make(
                "flag `" + flag + "` after an unquoted expression is ignored",
                "E0003", span, input, "unexpected trailing flag");
            d.help =
                "quote the expression so trailing flags are parsed: "
                "e.g.  :solve \"2x + 3 = 7\" --no-save";
            return d;
        }

        // Emitted when --method is used with a single polynomial equation,
        // where it has no effect (only meaningful for multi-equation systems).
        inline Diagnostic method_ignored_for_poly(const std::string& flag,
                                                  const std::string& input) {
            size_t pos  = input.rfind(flag);
            Span   span = (pos != std::string::npos)
                              ? Span(pos, pos + flag.size())
                              : Span{};
            auto d = Diagnostic::warning(
                "`" + flag +
                "` has no effect on single-equation polynomial solving");
            d.span  = span;
            d.input = input;
            d.inline_label = "ignored here";
            d.help  = "--method only applies to multi-equation systems "
                      "(e.g. :solve \"x+y=1; x-y=3\" --method=lu)";
            return d;
        }

    } // namespace errors

} // namespace math_solver
