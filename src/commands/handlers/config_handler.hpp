#pragma once

//! # Module — `src/commands/handlers/config_handler.hpp`
//!
//! Implements `handle_config` — the handler for `ConfigCommand` nodes (list,
//! get, set, path, reset). All logic is `inline` and lives entirely in this
//! header. Settings are validated against `Settings::all_keys()` /
//! `Settings::is_valid_key()` before any mutation occurs, and every successful
//! `Set` or `Reset` is persisted via `Config::save()`.

#include "ast/command/config_command.hpp"
#include "ast/command/history_entry.hpp"
#include "config/config.hpp"
#include "diagnostics/kinds/config_errors.hpp"
#include "diagnostics/sink.hpp"
#include "ui/color.hpp"
#include <sstream>

namespace math_solver {
    namespace handlers {

        /// Execute a `ConfigCommand` (list, get, set, path, reset) against `config`.
        ///
        /// Each action variant is handled as follows:
        /// - `List`  — Emits all settings and their current values, aligned.
        /// - `Get`   — Validates the key, then emits `key = value`.
        /// - `Set`   — Validates key and value, applies the change, calls
        ///   `config.save()`, and emits the updated value. For `auto_load_env`,
        ///   also verifies that the referenced environment exists.
        /// - `Path`  — Emits the on-disk config file path.
        /// - `Reset` — Restores all settings to defaults and calls `config.save()`.
        /// - `Unknown` — Emits `unknown_config_subcommand` diagnostic.
        ///
        /// # Arguments
        ///
        /// * `cmd`    — The configuration command to execute.
        /// * `config` — Live configuration store; mutated by `Set` and `Reset`.
        /// * `sink`   — Diagnostic sink for errors and output.
        ///
        /// # Returns
        ///
        /// `HistoryStatus::Info` for `List` and `Path`,
        /// `HistoryStatus::Success` for `Get`, `Set`, and `Reset`,
        /// `HistoryStatus::Error` on validation failure or `Unknown`.
        ///
        /// # Errors
        ///
        /// Pushes `missing_setting_key` when `Get` key is empty.
        /// Pushes `unknown_setting` when the key is not in `Settings::all_keys()`.
        /// Pushes `missing_key_or_value` when `Set` is missing key or value.
        /// Pushes `env_ref_error` when `auto_load_env` references a non-existent environment.
        /// Pushes `invalid_setting_value` when `Settings::set` rejects the value.
        /// Pushes `unknown_config_subcommand` for `Unknown` action.
        inline HistoryStatus handle_config(const ConfigCommand& cmd,
                                           Config&              config,
                                           DiagnosticSink&      sink) {
            const std::string& raw  = cmd.raw_command();
            const std::string& file = cmd.source_file();
            size_t             line = cmd.source_line();

            switch (cmd.action()) {

            case ConfigCommand::Action::List: {
                std::ostringstream oss;
                oss << "  " << ansi::bold << "Settings" << ansi::reset << "\n";
                for (const auto& key : Settings::all_keys()) {
                    std::string val = config.settings().get(key);
                    oss << "  " << ansi::dim << key;
                    for (size_t i = key.size(); i < 28; ++i)
                        oss << ' ';
                    oss << ansi::reset << val << "\n";
                }
                sink.push_output(oss.str());
                return HistoryStatus::Info;
            }

            case ConfigCommand::Action::Get: {
                const std::string& key = cmd.key();
                if (key.empty()) {
                    sink.push(errors::missing_setting_key(
                        raw, "get", "`config get <key>`", file, line));
                    return HistoryStatus::Error;
                }
                if (!Settings::is_valid_key(key)) {
                    sink.push(errors::unknown_setting(raw, key, file, line));
                    return HistoryStatus::Error;
                }
                std::ostringstream oss;
                oss << "  " << key << " = " << config.settings().get(key)
                    << "\n";
                sink.push_output(oss.str());
                return HistoryStatus::Success;
            }

            case ConfigCommand::Action::Set: {
                const std::string& key   = cmd.key();
                const std::string& value = cmd.value();

                if (key.empty() || value.empty()) {
                    sink.push(errors::missing_key_or_value(raw, file, line));
                    return HistoryStatus::Error;
                }
                if (!Settings::is_valid_key(key)) {
                    sink.push(errors::unknown_setting(raw, key, file, line));
                    return HistoryStatus::Error;
                }

                if (key == "auto_load_env" && !value.empty() &&
                    !config.env_exists(value)) {
                    sink.push(
                        errors::env_ref_error(raw, value, config, file, line));
                    return HistoryStatus::Error;
                }

                std::string err_msg = config.settings().set(key, value);
                if (!err_msg.empty()) {
                    sink.push(errors::invalid_setting_value(
                        raw, key, value, err_msg, file, line));
                    return HistoryStatus::Error;
                }

                config.save();
                std::ostringstream oss;
                oss << "  " << key << " = " << config.settings().get(key)
                    << "\n";
                sink.push_output(oss.str());
                return HistoryStatus::Success;
            }

            case ConfigCommand::Action::Path: {
                std::ostringstream oss;
                oss << "  " << config.file_path() << "\n";
                sink.push_output(oss.str());
                return HistoryStatus::Info;
            }

            case ConfigCommand::Action::Reset:
                config.reset_settings();
                config.save();
                sink.push_output("  Settings reset to defaults\n");
                return HistoryStatus::Success;

            case ConfigCommand::Action::Unknown:
                sink.push(errors::unknown_config_subcommand(raw, cmd.key(),
                                                            file, line));
                return HistoryStatus::Error;
            }

            return HistoryStatus::Unknown;
        }

    } // namespace handlers
} // namespace math_solver