#pragma once

#include "diagnostics/diagnostic.hpp"
#include "ui/suggestions.hpp"

#include <string>
#include <vector>

namespace math_solver {

    namespace command_kind {

        template <typename Cmd>
        inline SourceLocation loc_from_cmd(const Cmd& cmd) {
            return SourceLocation::from_file(cmd.source_file(),
                                             cmd.source_line());
        }

        template <typename Cmd>
        inline std::string raw_from_cmd(const Cmd& cmd) {
            return cmd.raw_command();
        }

        inline Diagnostic
        unknown_subcommand(const std::string& raw, const std::string& sub,
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

        inline Diagnostic missing_arg(const std::string& raw,
                                      const std::string& after_token,
                                      const std::string& usage,
                                      const std::string& file, size_t line) {
            auto d = Diagnostic::make("missing argument", "E0502",
                                      find_token_span(raw, after_token), raw,
                                      "argument expected here")
                         .with_location(file, line);
            d.help = "Usage: " + usage;
            return d;
        }

    } // namespace command_kind

} // namespace math_solver
