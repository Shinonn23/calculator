#pragma once

#include "config/config.hpp"
#include "core/span.hpp"
#include "diagnostics/diagnostic.hpp"
#include "ui/suggestions.hpp"

#include <string>
#include <vector>

namespace math_solver {

    namespace errors {

        inline Diagnostic env_not_found(const std::string& raw, const Span& span,
                                        const std::string& name,
                                        const Config&      config,
                                        const std::string& file, size_t line) {
            auto d = Diagnostic::make("environment `" + name + "` not found",
                                      "E0601", span, raw, "unknown environment")
                         .with_location(file, line);
            if (auto m = suggest(name, config.list_envs()))
                d.help =
                    "an environment with a similar name exists: `" + *m + "`";
            return d;
        }

        inline Diagnostic missing_env_name(const std::string& raw,
                                           const Span&        span,
                                           const std::string& usage,
                                           const std::string& file,
                                           size_t             line) {
            auto d = Diagnostic::make("missing name", "E0600", span, raw,
                                      "name expected here")
                         .with_location(file, line);
            d.help = "Usage: " + usage;
            return d;
        }

        inline Diagnostic
        unknown_env_subcommand(const std::string& raw, const std::string& sub,
                               const std::vector<std::string>& available,
                               const std::string& file, size_t line) {
            auto d = Diagnostic::make("unknown subcommand `" + sub + "`",
                                      "E0002", find_token_span(raw, sub), raw,
                                      "unrecognized subcommand")
                         .with_location(file, line);
            if (auto m = suggest(sub, available))
                d.help = "did you mean `" + *m + "`?";
            else {
                std::string s = "available:";
                for (const auto& a : available)
                    s += "  " + a;
                d.help = s;
            }
            return d;
        }

        inline Diagnostic var_skipped_warning(const std::string& raw,
                                              const Span&        span,
                                              const std::string& var_name,
                                              const std::string& file,
                                              size_t             line) {
            auto d = Diagnostic::warning("variable `" + var_name +
                                             "` not defined, skipped",
                                         span, raw)
                         .with_location(file, line);
            d.inline_label = "not in current context";
            return d;
        }

    } // namespace errors

} // namespace math_solver
