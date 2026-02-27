#pragma once

#include "ast/command/config_command.hpp"
#include "ast/command/history_entry.hpp"
#include "config/config.hpp"
#include "diagnostics/kinds/config_errors.hpp"
#include "diagnostics/sink.hpp"
#include "ui/color.hpp"

namespace math_solver {
    namespace handlers {

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