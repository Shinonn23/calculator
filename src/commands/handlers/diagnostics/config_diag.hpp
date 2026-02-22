#pragma once

#include "config/config.hpp"
#include "core/error.hpp"
#include "ui/suggestions.hpp"

namespace math_solver {
    namespace diag {

        // Handles the case where a user provides an unknown setting key.
        // - Suggests a similar key if available (fuzzy match).
        // - If no match, emits a note listing valid categories.
        // - Assumes Settings::all_keys() is up-to-date and exhaustive.
        inline DiagnosticBuilder unknown_setting(DiagnosticBuilder  base,
                                                 const std::string& key) {
            base.message      = "unknown setting `" + key + "`";
            base.span         = find_token_span(base.input, key);
            base.code         = "E0503";
            base.inline_label = "unknown setting key";
            if (auto m = suggest(key, Settings::all_keys()))
                base.help =
                    "a setting with a similar name exists: `" + *m + "`";
            else {
                base.note = "valid categories: output.*, solver.*, history.*, "
                            "repl.*, auto_load_env";
            }
            return base;
        }

        // Handles invalid value for a known setting key.
        // - `reason` is expected to be a precise error from Settings::set().
        // - Assumes caller has already validated the key.
        inline DiagnosticBuilder
        invalid_setting_value(DiagnosticBuilder base, const std::string& key,
                              const std::string& value,
                              const std::string& reason) {
            base.message      = "invalid value for `" + key + "`";
            base.span         = find_token_span(base.input, value);
            base.code         = "E0504";
            base.inline_label = "invalid value";
            base.note         = reason;
            return base;
        }

        // Warns when auto_load_env references a non-existent environment.
        // - Non-fatal: setting is still applied.
        // - Suggests similar env name if available.
        // - Assumes config.list_envs() is cheap and up-to-date.
        inline DiagnosticBuilder env_ref_not_found(DiagnosticBuilder  base,
                                                   const std::string& env_name,
                                                   const Config&      config) {
            base.level   = "warning";
            base.message = "environment `" + env_name + "` does not exist yet";
            base.span    = find_token_span(base.input, env_name);
            base.inline_label = "not yet created";
            base.note         = "create it with `:env new " + env_name + "`";
            if (auto m = suggest(env_name, config.list_envs()))
                base.help = "did you mean `" + *m + "`?";
            return base;
        }

        // Emits diagnostic for missing config key in a subcommand.
        // - Usage string must be accurate for the current CLI.
        // - Assumes subcommand is present in input.
        inline void emit_missing_config_key(const DiagnosticBuilder& base,
                                            const std::string&       subcommand,
                                            const std::string&       usage) {
            auto d         = base;
            d.message      = "missing setting key";
            d.span         = find_token_span(base.input, subcommand);
            d.code         = "E0502";
            d.inline_label = "key expected here";
            d.help         = "Usage: " + usage;
            std::cout << d.build();
        }

        // Emits diagnostic for missing key or value in config set.
        // - Usage string is hardcoded; update if CLI changes.
        inline void emit_missing_config_kv(const DiagnosticBuilder& base) {
            auto d         = base;
            d.message      = "missing key or value";
            d.span         = find_token_span(base.input, "set");
            d.code         = "E0502";
            d.inline_label = "key and value required";
            d.help         = "Usage: `:config set <key> <value>`";
            std::cout << d.build();
        }

        // Emits diagnostic for unknown setting key.
        // - Duplicates logic from unknown_setting for direct emission.
        // - Kept for legacy CLI compatibility.
        inline void emit_unknown_setting(const DiagnosticBuilder& base,
                                         const std::string&       key) {
            auto d         = base;
            d.message      = "unknown setting `" + key + "`";
            d.span         = find_token_span(base.input, key);
            d.code         = "E0503";
            d.inline_label = "unknown setting key";
            if (auto m = suggest(key, Settings::all_keys()))
                d.help = "a setting with a similar name exists: `" + *m + "`";
            else
                d.note = "valid categories: output.*, solver.*, history.*, "
                         "repl.*, auto_load_env";
            std::cout << d.build();
        }

        // Emits diagnostic for invalid value for a known setting key.
        // - `reason` should be a user-facing explanation.
        inline void emit_invalid_setting_value(const DiagnosticBuilder& base,
                                               const std::string&       key,
                                               const std::string&       value,
                                               const std::string& reason) {
            auto d         = base;
            d.message      = "invalid value for `" + key + "`";
            d.span         = find_token_span(base.input, value);
            d.code         = "E0504";
            d.inline_label = "invalid value";
            d.note         = reason;
            std::cout << d.build();
        }

        // Emits error for referencing a non-existent environment.
        // - Suggests similar env name if available.
        // - Assumes config.list_envs() is up-to-date.
        inline void emit_env_ref_error(const DiagnosticBuilder& base,
                                       const std::string&       env_name,
                                       const Config&            config) {
            auto d    = base;
            d.message = "environment `" + env_name + "` does not exist yet";
            d.span    = find_token_span(base.input, env_name);
            d.code    = "E0505";
            d.inline_label = "not yet created";
            d.note         = "create it with `:env new " + env_name + "`";
            if (auto m = suggest(env_name, config.list_envs()))
                d.help = "did you mean `" + *m + "`?";
            std::cout << d.build();
        }

        // Emits diagnostic for unknown config subcommand.
        // - Suggests similar subcommand if available.
        // - List of valid subcommands must be kept in sync with CLI.
        inline void
        emit_unknown_config_subcommand(const DiagnosticBuilder& base,
                                       const std::string&       sub) {
            static const std::vector<std::string> subs = {"list", "get", "set",
                                                          "path", "reset"};
            auto                                  d    = base;
            d.message      = "unknown config subcommand `" + sub + "`";
            d.span         = find_token_span(base.input, sub);
            d.code         = "E0002";
            d.inline_label = "unrecognized subcommand";
            if (auto m = suggest(sub, subs))
                d.help = "did you mean `" + *m + "`?";
            else
                d.help = "available: list, get, set, path, reset";
            std::cout << d.build();
        }

    } // namespace diag
} // namespace math_solver