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
        // Handles all config-related commands. This function is the sole entry
        // point for configuration mutation and querying from the command
        // interface. All changes are persisted immediately to disk to avoid
        // state desynchronization.
        //
        // Invariants:
        // - The config object must be valid and up-to-date before invocation.
        // - All setting keys must be validated against Settings::all_keys().
        // - Error reporting must provide actionable diagnostics for invalid
        // input.
        //
        // Subtlety:
        // - The 'auto_load_env' key triggers a warning if the referenced
        // environment
        //   does not exist, but does not prevent the setting from being
        //   applied.
        // - All output is written directly to std::cout; this is assumed to be
        //   serialized by the caller if concurrency is introduced.
        inline HistoryStatus handle_config(const ConfigCommand& cmd,
                                           Config&              config) {
            using std::cout;
            const std::string& raw = cmd.raw_command();

            switch (cmd.action()) {

            case ConfigCommand::Action::List:
                // List all settings and their current values. Output is aligned
                // for readability.
                cout << "  " << ansi::bold << "Settings" << ansi::reset << "\n";
                for (const auto& key : Settings::all_keys()) {
                    std::string val = config.settings().get(key);
                    cout << "  " << ansi::dim << key;
                    for (size_t i = key.size(); i < 16; ++i)
                        cout << ' ';
                    cout << ansi::reset << val << "\n";
                }
                return HistoryStatus::Info;

            case ConfigCommand::Action::Get: {
                // Query a single setting. Returns an error if the key is
                // missing or invalid.
                const std::string& key = cmd.key();
                if (key.empty()) {
                    MathError err("missing setting key",
                                  find_token_span(raw, "get"), raw);
                    err.with_code("E0502").with_help(
                        "Usage: `:config get <key>`");
                    cout << err.format();
                    return HistoryStatus::Error;
                }
                if (!Settings::is_valid_key(key)) {
                    MathError err("unknown setting `" + key + "`",
                                  find_token_span(raw, key), raw);
                    err.with_code("E0503").with_label("unknown setting key");

                    auto match = suggest(key, Settings::all_keys());
                    if (match) {
                        err.with_help(
                            "a setting with a similar name exists: `" + *match +
                            "`");
                    }
                    cout << err.format();
                    return HistoryStatus::Error;
                }
                cout << "  " << key << " = " << config.settings().get(key)
                     << "\n";
                return HistoryStatus::Success;
            }

            case ConfigCommand::Action::Set: {
                // Set a configuration key to a new value. All validation is
                // performed prior to mutation. If the value is invalid, the
                // error message from Settings::set is surfaced to the user.
                //
                // Notably, for 'auto_load_env', we warn if the environment does
                // not exist, but still apply the setting. This is to avoid
                // breaking workflows that rely on deferred environment
                // creation.
                const std::string& key   = cmd.key();
                const std::string& value = cmd.value();

                if (key.empty() || value.empty()) {
                    MathError err("missing key or value",
                                  find_token_span(raw, "set"), raw);
                    err.with_code("E0502").with_help(
                        "Usage: `:config set <key> <value>`");
                    cout << err.format();
                    return HistoryStatus::Error;
                }

                if (!Settings::is_valid_key(key)) {
                    MathError err("unknown setting `" + key + "`",
                                  find_token_span(raw, key), raw);
                    err.with_code("E0503").with_label("unknown setting key");

                    auto match = suggest(key, Settings::all_keys());
                    if (match) {
                        err.with_help(
                            "a setting with a similar name exists: `" + *match +
                            "`");
                    }
                    cout << err.format();
                    return HistoryStatus::Error;
                }

                std::string err_msg = config.settings().set(key, value);
                if (!err_msg.empty()) {
                    MathError err("invalid value for `" + key + "`: " + err_msg,
                                  find_token_span(raw, value), raw);
                    err.with_code("E0504").with_label("invalid value");
                    cout << err.format();
                    return HistoryStatus::Error;
                }

                bool has_warning = false;
                if (key == "auto_load_env" && !config.env_exists(value)) {
                    // Warn if the referenced environment does not exist yet.
                    cout << ansi::yellow << "  Warning: " << ansi::reset
                         << "environment '" << value
                         << "' does not exist yet\n";
                    has_warning = true;
                }

                config.save();
                cout << "  " << key << " = " << config.settings().get(key)
                     << "\n";

                // If a warning was issued, propagate as HistoryStatus::Warning.
                return has_warning ? HistoryStatus::Warning
                                   : HistoryStatus::Success;
            }

            case ConfigCommand::Action::Path:
                // Print the path to the config file. Used for debugging and
                // external tooling.
                cout << "  " << config.file_path() << "\n";
                return HistoryStatus::Info;

            case ConfigCommand::Action::Reset:
                // Reset all settings to defaults and persist immediately.
                config.reset_settings();
                config.save();
                cout << "  Settings reset to defaults\n";
                return HistoryStatus::Success;

            case ConfigCommand::Action::Unknown: {
                std::string         input   = cmd.raw_command();
                std::string         bad_cmd = input.substr(0, input.find(' '));

                UnknownCommandError e       = UnknownCommandError(
                    bad_cmd, find_token_span(input, bad_cmd), input);

                cout << e.format() << "\n";
                return HistoryStatus::Error;
            }
            }

            // Defensive: should be unreachable unless new actions are added
            // without updating this handler.
            return HistoryStatus::Unknown;
        }

    } // namespace handlers
} // namespace math_solver