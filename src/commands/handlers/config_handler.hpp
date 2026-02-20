#pragma once

#include "ast/command/config_command.hpp"
#include "config/config.hpp"
#include "ui/color.hpp"
#include "ui/suggestions.hpp"

#include <iostream>
#include <string>

namespace math_solver {
    namespace handlers {

        // Entry point for handling all config-related commands.
        // - Assumes `config` is a valid, mutable configuration object.
        // - All output is written directly to std::cout; this is intentional to
        //   ensure immediate user feedback (see issue #142).
        // - The switch on cmd.action() must remain exhaustive as new actions
        // are added.
        inline void handle_config(const ConfigCommand& cmd, Config& config) {
            using std::cout;

            switch (cmd.action()) {

            case ConfigCommand::Action::List: {
                // List all settings and their current values.
                // - Output is column-aligned for readability.
                // - Assumes Settings::all_keys() returns a stable, complete
                // set.
                cout << "  " << ansi::bold << "Settings" << ansi::reset << "\n";
                for (const auto& key : Settings::all_keys()) {
                    std::string val = config.settings().get(key);
                    cout << "  " << ansi::dim << key;
                    for (size_t i = key.size(); i < 16; ++i)
                        cout << ' ';
                    cout << ansi::reset << val << "\n";
                }
                break;
            }

            case ConfigCommand::Action::Get: {
                // Fetch and display the value for a specific key.
                // - If key is missing, print usage and available keys.
                // - If key is invalid, print error and suggest alternatives.
                const std::string& key = cmd.key();
                if (key.empty()) {
                    cout << "  Usage: :config get <key>\n";
                    cout << "  Keys: ";
                    for (const auto& k : Settings::all_keys())
                        cout << k << " ";
                    cout << "\n";
                    break;
                }
                if (!Settings::is_valid_key(key)) {
                    cout << ansi::red << "  Error: " << ansi::reset
                         << "unknown setting '" << key << "'\n";
                    maybe_suggest_setting(key);
                    break;
                }
                cout << "  " << key << " = " << config.settings().get(key)
                     << "\n";
                break;
            }

            case ConfigCommand::Action::Set: {
                // Set a configuration key to a new value.
                // - Rejects empty key or value.
                // - Validates key before attempting to set.
                // - If setting fails, prints error from settings().set().
                // - Special-case: if setting "auto_load_env" to a non-existent
                // environment,
                //   warn but do not prevent the change (see #203).
                // - Always persists changes to disk after mutation.
                const std::string& key   = cmd.key();
                const std::string& value = cmd.value();
                if (key.empty() || value.empty()) {
                    cout << "  Usage: :config set <key> <value>\n";
                    break;
                }
                if (!Settings::is_valid_key(key)) {
                    cout << ansi::red << "  Error: " << ansi::reset
                         << "unknown setting '" << key << "'\n";
                    maybe_suggest_setting(key);
                    break;
                }
                std::string err = config.settings().set(key, value);
                if (!err.empty()) {
                    cout << ansi::red << "  Error: " << ansi::reset << err
                         << "\n";
                    break;
                }
                if (key == "auto_load_env" && !config.env_exists(value))
                    cout << ansi::yellow << "  Warning: " << ansi::reset
                         << "environment '" << value
                         << "' does not exist yet\n";
                config.save();
                cout << "  " << key << " = " << config.settings().get(key)
                     << "\n";
                break;
            }

            case ConfigCommand::Action::Path:
                // Print the path to the current config file.
                // - Used for diagnostics and debugging.
                cout << "  " << config.file_path() << "\n";
                break;

            case ConfigCommand::Action::Reset:
                // Reset all settings to their default values.
                // - Persists the reset immediately.
                // - No confirmation prompt; caller is responsible for any
                // UI-level confirmation.
                config.reset_settings();
                config.save();
                cout << "  Settings reset to defaults\n";
                break;
            }
        }

    } // namespace handlers
} // namespace math_solver