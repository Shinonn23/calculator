#pragma once

#include "diagnostics/diagnostic.hpp"
#include "ui/suggestions.hpp"

#include <string>
#include <vector>

namespace math_solver {

    namespace errors {

        inline Diagnostic history_range_error(const std::string& raw,
                                              const std::string& err_msg,
                                              const Span&        span,
                                              const std::string& file,
                                              size_t             line) {
            return Diagnostic::make(err_msg, "E0701", span, raw,
                                    "invalid range")
                .with_location(file, line);
        }

        inline Diagnostic history_missing_arg(const std::string& raw,
                                              const std::string& after_token,
                                              const std::string& usage,
                                              const std::string& file,
                                              size_t             line) {
            auto d = Diagnostic::make("missing argument", "E0702",
                                      find_token_span(raw, after_token), raw,
                                      "argument expected here")
                         .with_location(file, line);
            d.help = "Usage: " + usage;
            return d;
        }

        inline Diagnostic history_write_error(const std::string& raw,
                                              const std::string& filepath,
                                              const std::string& file,
                                              size_t             line) {
            return Diagnostic::make("cannot write to '" + filepath + "'",
                                    "E0703", find_token_span(raw, filepath),
                                    raw, "permission denied or path invalid")
                .with_location(file, line);
        }

        inline Diagnostic unknown_history_subcommand(const std::string& raw,
                                                     const std::string& sub,
                                                     const std::string& file,
                                                     size_t             line) {
            static const std::vector<std::string> subs = {"show", "search",
                                                          "save", "clear"};
            auto                                  d =
                Diagnostic::make("unknown history subcommand `" + sub + "`",
                                 "E0002", find_token_span(raw, sub), raw,
                                 "unrecognized subcommand")
                    .with_location(file, line);
            if (auto m = suggest(sub, subs))
                d.help = "did you mean `" + *m + "`?";
            else
                d.help = "available: show, search, save, clear";
            return d;
        }

    } // namespace errors

} // namespace math_solver
