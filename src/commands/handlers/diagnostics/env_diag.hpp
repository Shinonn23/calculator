#pragma once

// Env-specific diagnostics.

#include "command_diag.hpp"
#include "config/config.hpp"

namespace math_solver {
namespace diag {

    // E0601 — environment not found, with suggestion from config.list_envs().
    inline DiagnosticBuilder env_not_found(DiagnosticBuilder  base,
                                           const std::string& name,
                                           const Config&      config) {
        return not_found(base, "environment", name, config.list_envs(), "E0601");
    }

    // E0602 — cannot delete or move the active environment.
    inline DiagnosticBuilder active_env_protected(DiagnosticBuilder  base,
                                                   const std::string& name,
                                                   const std::string& action) {
        base.message      = "cannot " + action + " the active environment";
        base.span         = find_token_span(base.input, name);
        base.code         = "E0602";
        base.inline_label = "active environment";
        base.help         = "switch first with `:env load <name>`";
        return base;
    }

    // Warning — variable not found when saving/moving a subset of variables.
    inline DiagnosticBuilder var_skipped(DiagnosticBuilder  base,
                                         const std::string& var_name) {
        base.level        = "warning";
        base.message      = "variable `" + var_name + "` not defined, skipped";
        base.span         = find_token_span(base.input, var_name);
        base.inline_label = "not in current context";
        return base;
    }

} // namespace diag
} // namespace math_solver