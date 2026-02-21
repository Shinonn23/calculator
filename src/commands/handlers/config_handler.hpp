#pragma once

#include "ast/command/config_command.hpp"
#include "ast/command/history_entry.hpp"
#include "config/config.hpp"
#include "core/error.hpp"
#include "ui/color.hpp"
#include "ui/suggestions.hpp"

#include <iostream>
#include <string>

namespace math_solver {
    namespace handlers {

        // Handles all config-related commands. This function is the single
        // entry point for config command dispatch. Assumes `config` is a valid,
        // mutable reference to the current configuration state. All output is
        // written to std::cout.
        //
        // Invariants:
        // - `cmd` must be a well-formed ConfigCommand as parsed by the
        // frontend.
        // - `config` must be in a consistent state; mutations are persisted via
        // `save()`.
        // - Returns a HistoryStatus reflecting the outcome for integration with
        // the
        //   command history subsystem.
        //
        // Subtlety:
        // - Error diagnostics are constructed with source location for IDE
        // integration.
        // - Suggestion logic is used for both keys and subcommands to improve
        // UX.
        // - The "auto_load_env" key triggers a warning if the referenced
        // environment
        //   does not exist, but does not block the operation.
        inline HistoryStatus handle_config(const ConfigCommand& cmd,
                                           Config&              config) {
            using std::cout;
            const std::string& raw       = cmd.raw_command();

            // Always attach source location to diagnostics for downstream
            // consumers.
            auto               make_diag = [&]() {
                DiagnosticBuilder d;
                d.input     = raw;
                d.filename  = cmd.source_file();
                d.file_line = cmd.source_line();
                return d;
            };

            switch (cmd.action()) {

            case ConfigCommand::Action::List:
                // List all settings and their current values. Output is
                // column-aligned for readability. Assumes Settings::all_keys()
                // is stable and exhaustive.
                cout << "  " << ansi::bold << "Settings" << ansi::reset << "\n";
                for (const auto& key : Settings::all_keys()) {
                    std::string val = config.settings().get(key);
                    cout << "  " << ansi::dim << key;
                    for (size_t i = key.size(); i < 28; ++i)
                        cout << ' ';
                    cout << ansi::reset << val << "\n";
                }
                return HistoryStatus::Info;

            case ConfigCommand::Action::Get: {
                // Handles `:config get <key>`. Validates key presence and
                // existence. Suggests similar keys on error. Returns error
                // diagnostics with precise span information for editor
                // integration.
                const std::string& key = cmd.key();
                if (key.empty()) {
                    auto d         = make_diag();
                    d.message      = "missing setting key";
                    d.span         = find_token_span(raw, "get");
                    d.code         = "E0502";
                    d.inline_label = "key expected here";
                    d.help         = "Usage: `:config get <key>`";
                    cout << d.build();
                    return HistoryStatus::Error;
                }
                if (!Settings::is_valid_key(key)) {
                    auto d         = make_diag();
                    d.message      = "unknown setting `" + key + "`";
                    d.span         = find_token_span(raw, key);
                    d.code         = "E0503";
                    d.inline_label = "unknown setting key";
                    if (auto match = suggest(key, Settings::all_keys()))
                        d.help = "a setting with a similar name exists: `" +
                                 *match + "`";
                    cout << d.build();
                    return HistoryStatus::Error;
                }
                cout << "  " << key << " = " << config.settings().get(key)
                     << "\n";
                return HistoryStatus::Success;
            }

            case ConfigCommand::Action::Set: {
                // Handles `:config set <key> <value>`. Performs validation on
                // both key and value. If the key is "auto_load_env", emits a
                // warning if the environment does not exist, but still applies
                // the setting. All mutations are persisted immediately to disk.
                const std::string& key   = cmd.key();
                const std::string& value = cmd.value();

                if (key.empty() || value.empty()) {
                    auto d         = make_diag();
                    d.message      = "missing key or value";
                    d.span         = find_token_span(raw, "set");
                    d.code         = "E0502";
                    d.inline_label = "key and value required";
                    d.help         = "Usage: `:config set <key> <value>`";
                    cout << d.build();
                    return HistoryStatus::Error;
                }
                if (!Settings::is_valid_key(key)) {
                    auto d         = make_diag();
                    d.message      = "unknown setting `" + key + "`";
                    d.span         = find_token_span(raw, key);
                    d.code         = "E0503";
                    d.inline_label = "unknown setting key";
                    if (auto match = suggest(key, Settings::all_keys()))
                        d.help = "a setting with a similar name exists: `" +
                                 *match + "`";
                    cout << d.build();
                    return HistoryStatus::Error;
                }

                std::string err_msg = config.settings().set(key, value);
                if (!err_msg.empty()) {
                    // Value failed validation; propagate error from settings
                    // layer.
                    auto d         = make_diag();
                    d.message      = "invalid value for `" + key + "`";
                    d.span         = find_token_span(raw, value);
                    d.code         = "E0504";
                    d.inline_label = "invalid value";
                    d.note         = err_msg;
                    cout << d.build();
                    return HistoryStatus::Error;
                }

                bool has_warning = false;
                if (key == "auto_load_env" && !value.empty() &&
                    !config.env_exists(value)) {
                    // Warn if the referenced environment does not exist yet.
                    // This is non-fatal; the user may intend to create it
                    // later.
                    auto d  = make_diag();
                    d.level = "warning";
                    d.message =
                        "environment `" + value + "` does not exist yet";
                    d.span         = find_token_span(raw, value);
                    d.inline_label = "not yet created";
                    d.note         = "create it with `:env new " + value + "`";
                    if (auto match = suggest(value, config.list_envs()))
                        d.help = "did you mean `" + *match + "`?";
                    cout << d.build();
                    has_warning = true;
                }

                config.save();
                cout << "  " << key << " = " << config.settings().get(key)
                     << "\n";
                return has_warning ? HistoryStatus::Warning
                                   : HistoryStatus::Success;
            }

            case ConfigCommand::Action::Path:
                // Returns the path to the current config file. Used for
                // debugging and external tooling.
                cout << "  " << config.file_path() << "\n";
                return HistoryStatus::Info;

            case ConfigCommand::Action::Reset:
                // Resets all settings to their default values and persists
                // immediately. No confirmation is required; assumes caller has
                // already prompted user.
                config.reset_settings();
                config.save();
                cout << "  Settings reset to defaults\n";
                return HistoryStatus::Success;

            case ConfigCommand::Action::Unknown: {
                // Handles unknown subcommands. Suggests similar subcommands if
                // possible. Returns a diagnostic with available options for
                // discoverability.
                const std::string& sub = cmd.key();
                auto               d   = make_diag();
                d.message      = "unknown config subcommand `" + sub + "`";
                d.span         = find_token_span(raw, sub);
                d.code         = "E0002";
                d.inline_label = "unrecognized subcommand";
                static const std::vector<std::string> subs = {
                    "list", "get", "set", "path", "reset"};
                if (auto match = suggest(sub, subs))
                    d.help = "did you mean `" + *match + "`?";
                else
                    d.help = "available: list, get, set, path, reset";
                cout << d.build();
                return HistoryStatus::Error;
            }
            }

            // Defensive: should be unreachable unless a new Action is added
            // without updating this switch. Return Unknown for forward
            // compatibility.
            return HistoryStatus::Unknown;
        }

    } // namespace handlers
} // namespace math_solver