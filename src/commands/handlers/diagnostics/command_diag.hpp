#pragma once

#include "core/error.hpp"
#include "ui/suggestions.hpp"

#include <string>
#include <vector>

namespace math_solver {
    namespace diag {

        // Constructs a DiagnosticBuilder with source location from a Command.
        // - Assumes Cmd provides raw_command(), source_file(), and
        // source_line().
        // - Used as the canonical entry for diagnostics to ensure consistent
        //   source mapping across all handlers.
        template <typename Cmd>
        inline DiagnosticBuilder from_cmd(const Cmd& cmd) {
            DiagnosticBuilder d;
            d.input     = cmd.raw_command();
            d.filename  = cmd.source_file();
            d.file_line = cmd.source_line();
            return d;
        }

        // Reports an unknown subcommand error.
        // - Suggests the closest match if edit distance is small enough.
        // - If no close match, emits a list of all available subcommands.
        // - Assumes `sub` is a token present in `base.input`.
        // - The edit distance threshold is determined by suggest().
        inline DiagnosticBuilder
        unknown_subcommand(DiagnosticBuilder base, const std::string& sub,
                           const std::vector<std::string>& available) {
            base.message      = "unknown subcommand `" + sub + "`";
            base.span         = find_token_span(base.input, sub);
            base.code         = "E0002";
            base.inline_label = "unrecognized subcommand";
            if (auto m = suggest(sub, available))
                base.help = "did you mean `" + *m + "`?";
            else {
                std::string s = "available:";
                for (const auto& a : available)
                    s += "  " + a;
                base.help = s;
            }
            return base;
        }

        // Reports a missing argument error.
        // - `after_token` is assumed to be the last valid token before the
        // missing argument.
        // - The caret is positioned after `after_token` for clarity.
        // - Usage string is provided for context; correctness of usage is the
        // caller's responsibility.
        inline DiagnosticBuilder missing_arg(DiagnosticBuilder  base,
                                             const std::string& after_token,
                                             const std::string& usage) {
            base.message      = "missing argument";
            base.span         = find_token_span(base.input, after_token);
            base.code         = "E0502";
            base.inline_label = "argument expected here";
            base.help         = "Usage: " + usage;
            return base;
        }

        // Reports a missing name error (e.g., environment, file, etc.).
        // - Used when the missing argument is specifically a "name".
        // - Otherwise identical to missing_arg.
        inline DiagnosticBuilder missing_name(DiagnosticBuilder  base,
                                              const std::string& after_token,
                                              const std::string& usage) {
            base.message      = "missing name";
            base.span         = find_token_span(base.input, after_token);
            base.code         = "E0600";
            base.inline_label = "name expected here";
            base.help         = "Usage: " + usage;
            return base;
        }

        // Reports an entity-not-found error, with fuzzy suggestion if
        // available.
        // - `kind` is a human-readable label (e.g., "environment", "variable").
        // - Suggestion is only provided if edit distance is small enough.
        // - Assumes `name` is present in `base.input`.
        // - The error code must be provided by the caller to allow for
        // domain-specific codes.
        inline DiagnosticBuilder
        not_found(DiagnosticBuilder base, const std::string& kind,
                  const std::string&              name,
                  const std::vector<std::string>& candidates,
                  const std::string&              code) {
            base.message      = kind + " `" + name + "` not found";
            base.span         = find_token_span(base.input, name);
            base.code         = code;
            base.inline_label = "unknown " + kind;
            if (auto m = suggest(name, candidates))
                base.help =
                    "a " + kind + " with a similar name exists: `" + *m + "`";
            return base;
        }

        // Reports a runtime error surfaced from std::exception at a known
        // token.
        // - Used for unexpected failures during command execution.
        // - The error code is provided by the caller.
        // - The caret is positioned at `at_token` for context.
        inline DiagnosticBuilder runtime_error(DiagnosticBuilder  base,
                                               const std::string& message,
                                               const std::string& at_token,
                                               const std::string& code) {
            base.message      = message;
            base.span         = find_token_span(base.input, at_token);
            base.code         = code;
            base.inline_label = "operation failed";
            return base;
        }

        // Constructs a warning diagnostic.
        // - Uses the same infrastructure as errors, but sets level to
        // "warning".
        // - Optionally attaches a note for additional context.
        // - The caller is responsible for ensuring that warnings are not
        //   suppressed or misclassified as errors.
        inline DiagnosticBuilder warning(DiagnosticBuilder  base,
                                         const std::string& message,
                                         const std::string& at_token,
                                         const std::string& note_text = "") {
            base.level        = "warning";
            base.message      = message;
            base.span         = find_token_span(base.input, at_token);
            base.inline_label = "potential issue";
            if (!note_text.empty())
                base.note = note_text;
            return base;
        }

    } // namespace diag
} // namespace math_solver