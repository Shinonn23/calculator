#pragma once

#include "config/config.hpp"
#include "diagnostics/diagnostic.hpp"
#include "ui/suggestions.hpp"

#include <string>
#include <vector>

namespace math_solver {

    namespace errors {

        inline Diagnostic missing_setting_key(const std::string& raw,
                                              const std::string& at_token,
                                              const std::string& usage,
                                              const std::string& file,
                                              size_t             line) {
            auto d = Diagnostic::make("missing setting key", "E0502",
                                      find_token_span(raw, at_token), raw,
                                      "key expected here")
                         .with_location(file, line);
            d.help = "Usage: " + usage;
            return d;
        }

        inline Diagnostic missing_key_or_value(const std::string& raw,
                                               const std::string& file,
                                               size_t             line) {
            auto d = Diagnostic::make("missing key or value", "E0502",
                                      find_token_span(raw, "set"), raw,
                                      "key and value required")
                         .with_location(file, line);
            d.help = "Usage: `:config set <key> <value>`";
            return d;
        }

        inline Diagnostic unknown_setting(const std::string& raw,
                                          const std::string& key,
                                          const std::string& file,
                                          size_t             line) {
            auto d = Diagnostic::make("unknown setting `" + key + "`", "E0503",
                                      find_token_span(raw, key), raw,
                                      "unknown setting key")
                         .with_location(file, line);
            if (auto m = suggest(key, Settings::all_keys()))
                d.help = "a setting with a similar name exists: `" + *m + "`";
            else
                d.note = "valid categories: output.*, solver.*, history.*, "
                         "repl.*, auto_load_env";
            return d;
        }

        inline Diagnostic invalid_setting_value(const std::string& raw,
                                                const std::string& key,
                                                const std::string& value,
                                                const std::string& reason,
                                                const std::string& file,
                                                size_t             line) {
            auto d = Diagnostic::make("invalid value for `" + key + "`",
                                      "E0504", find_token_span(raw, value), raw,
                                      "invalid value")
                         .with_location(file, line);
            d.note = reason;
            return d;
        }

        inline Diagnostic env_ref_error(const std::string& raw,
                                        const std::string& env_name,
                                        const Config&      config,
                                        const std::string& file, size_t line) {
            auto d = Diagnostic::make("environment `" + env_name +
                                          "` does not exist yet",
                                      "E0505", find_token_span(raw, env_name),
                                      raw, "not yet created")
                         .with_location(file, line);
            d.note = "create it with `:env new " + env_name + "`";
            if (auto m = suggest(env_name, config.list_envs()))
                d.help = "did you mean `" + *m + "`?";
            return d;
        }

        inline Diagnostic unknown_config_subcommand(const std::string& raw,
                                                    const std::string& sub,
                                                    const std::string& file,
                                                    size_t             line) {
            static const std::vector<std::string> subs = {"list", "get", "set",
                                                          "path", "reset"};
            auto d = Diagnostic::make("unknown config subcommand `" + sub + "`",
                                      "E0002", find_token_span(raw, sub), raw,
                                      "unrecognized subcommand")
                         .with_location(file, line);
            if (auto m = suggest(sub, subs))
                d.help = "did you mean `" + *m + "`?";
            else
                d.help = "available: list, get, set, path, reset";
            return d;
        }

    } // namespace errors

} // namespace math_solver
