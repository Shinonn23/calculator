#pragma once

#include "core/error.hpp"
#include "ui/suggestions.hpp"

#include <iostream>
#include <string>

namespace math_solver {
namespace diag {

    // E0701 — index out of range in history selector.
    // `span` is computed by caller since it depends on raw parsing context.
    inline void emit_history_range_error(const DiagnosticBuilder& base,
                                         const std::string&       err_msg,
                                         const Span&              span) {
        auto d         = base;
        d.message      = err_msg;
        d.span         = span;
        d.code         = "E0701";
        d.inline_label = "invalid range";
        std::cout << d.build();
    }

    // E0702 — missing required argument (pattern, filepath).
    inline void emit_history_missing_arg(const DiagnosticBuilder& base,
                                         const std::string&       after_token,
                                         const std::string&       usage) {
        auto d         = base;
        d.message      = "missing argument";
        d.span         = find_token_span(base.input, after_token);
        d.code         = "E0702";
        d.inline_label = "argument expected here";
        d.help         = "Usage: " + usage;
        std::cout << d.build();
    }

    // E0703 — file write failure.
    inline void emit_history_write_error(const DiagnosticBuilder& base,
                                         const std::string&       filepath) {
        auto d         = base;
        d.message      = "cannot write to '" + filepath + "'";
        d.span         = find_token_span(base.input, filepath);
        d.code         = "E0703";
        d.inline_label = "permission denied or path invalid";
        std::cout << d.build();
    }

    // E0002 — unknown history subcommand.
    inline void emit_unknown_history_subcommand(const DiagnosticBuilder& base,
                                                const std::string&       sub) {
        static const std::vector<std::string> subs =
            {"show", "search", "save", "clear"};
        auto d         = base;
        d.message      = "unknown history subcommand `" + sub + "`";
        d.span         = find_token_span(base.input, sub);
        d.code         = "E0002";
        d.inline_label = "unrecognized subcommand";
        if (auto m = suggest(sub, subs))
            d.help = "did you mean `" + *m + "`?";
        else
            d.help = "available: show, search, save, clear";
        std::cout << d.build();
    }

} // namespace diag
} // namespace math_solver