#pragma once

// Var-specific diagnostics.

#include "command_diag.hpp"
#include "runtime/context/context.hpp"

namespace math_solver {
namespace diag {

    // E0425 — variable not found, with suggestion from context.
    inline DiagnosticBuilder var_not_found(DiagnosticBuilder  base,
                                           const std::string& name,
                                           const Context&     ctx) {
        return not_found(base, "variable", name, ctx.all_names(), "E0425");
    }

    // E0401 — missing variable name in :set/:unset.
    inline DiagnosticBuilder missing_var_name(DiagnosticBuilder  base,
                                              const std::string& cmd_token,
                                              const std::string& usage) {
        base.message      = "missing variable name";
        base.span         = find_token_span(base.input, cmd_token);
        base.code         = "E0401";
        base.inline_label = "variable name expected here";
        base.help         = "Usage: " + usage;
        return base;
    }

    // E0402 — reserved keyword used as variable name.
    inline DiagnosticBuilder reserved_keyword(DiagnosticBuilder  base,
                                              const std::string& name) {
        base.message      = "`" + name + "` is a reserved keyword";
        base.span         = find_token_span(base.input, name);
        base.code         = "E0402";
        base.inline_label = "reserved word";
        base.help         = "choose a different variable name";
        return base;
    }

    // E0403 — invalid identifier format.
    inline DiagnosticBuilder invalid_identifier(DiagnosticBuilder  base,
                                                const std::string& name) {
        base.message      = "invalid variable name `" + name + "`";
        base.span         = find_token_span(base.input, name);
        base.code         = "E0403";
        base.inline_label = "invalid identifier";
        base.help         = "names must start with a letter or `_` and "
                            "contain only alphanumeric characters";
        return base;
    }

    // E0404 — missing expression after variable name in :set.
    inline DiagnosticBuilder missing_expr(DiagnosticBuilder  base,
                                          const std::string& var_name) {
        base.message      = "missing expression";
        base.span         = find_token_span(base.input, var_name);
        base.code         = "E0404";
        base.inline_label = "expression expected after this";
        base.help         = "Usage: `:set <var> <expr>`";
        return base;
    }

} // namespace diag
} // namespace math_solver