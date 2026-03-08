#pragma once

#include "core/span.hpp"
#include <cassert>
#include <string>

namespace math_solver {

    // Returns the span of the first occurrence of `token` in `raw`.
    // - Assumes both `raw` and `token` are valid UTF-8.
    // - Returns empty span if not found or token is empty.
    // - Used for error reporting to highlight relevant source fragments.
    inline Span find_token_span(const std::string& raw,
                                const std::string& token) {
        if (token.empty())
            return Span();
        size_t pos = raw.find(token);
        if (pos != std::string::npos) {
            return Span(pos, pos + token.length());
        }
        return Span();
    }

    struct SourceLocation {
        std::string file = "<repl>";
        size_t      line = 1;
        size_t      col  = 1;

        static SourceLocation repl() { return {}; }
        static SourceLocation from_file(const std::string& f, size_t l) {
            return {f, l, 1};
        }
        bool is_repl() const { return file == "<repl>"; }
    };

    struct Diagnostic {
        // ── Core fields (set at throw/construction site) ──────────────────
        std::string message;
        std::string level        = "error"; // "error" | "warning"
        std::string code         = "";
        std::string inline_label = "";
        Span        span;
        std::string input; // raw source line

        // ── Augmentation fields (set at catch/handler site) ───────────────
        std::string    help = "";
        std::string    note = "";
        SourceLocation loc;

        static Diagnostic make(const std::string& msg,
                               const std::string& code  = "",
                               const Span&        span  = {},
                               const std::string& input = "",
                               const std::string& label = "");

        static Diagnostic warning(const std::string& msg,
                                  const Span&        span  = {},
                                  const std::string& input = "") {
            Diagnostic d;
            d.message = msg;
            d.level   = "warning";
            d.span    = span;
            d.input   = input;
            return d;
        }

        [[nodiscard]] Diagnostic with_help(const std::string& h) const {
            Diagnostic copy = *this;
            copy.help       = h;
            return copy;
        }
        [[nodiscard]] Diagnostic with_note(const std::string& n) const {
            Diagnostic copy = *this;
            copy.note       = n;
            return copy;
        }
        [[nodiscard]] Diagnostic with_label(const std::string& l) const {
            Diagnostic copy     = *this;
            copy.inline_label   = l;
            return copy;
        }
        [[nodiscard]] Diagnostic with_location(const SourceLocation& l) const {
            Diagnostic copy = *this;
            copy.loc        = l;
            return copy;
        }
        [[nodiscard]] Diagnostic with_location(const std::string& file,
                                               size_t             line) const {
            if (file.empty() && line == 0)
                return *this;
            return with_location(SourceLocation::from_file(file, line));
        }

        std::string format() const;
    };

} // namespace math_solver
