#pragma once

#include "ast/command/config_command.hpp"
#include "ast/command/history_entry.hpp"
#include "commands/handlers/diagnostics/command_diag.hpp"
#include "commands/handlers/diagnostics/config_diag.hpp"
#include "config/config.hpp"
#include "core/diagnostic_sink.hpp"
#include "ui/color.hpp"

#include <iostream>

namespace math_solver {
    namespace handlers {

        inline HistoryStatus handle_config(const ConfigCommand& cmd,
                                           Config&              config,
                                           DiagnosticSink& /*sink*/) {
            using std::cout;
            const auto base = diag::from_cmd(cmd);

            switch (cmd.action()) {

            case ConfigCommand::Action::List:
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
                const std::string& key = cmd.key();
                if (key.empty()) {
                    diag::emit_missing_config_key(base, "get",
                                                  "`config get <key>`");
                    return HistoryStatus::Error;
                }
                if (!Settings::is_valid_key(key)) {
                    diag::emit_unknown_setting(base, key);
                    return HistoryStatus::Error;
                }
                cout << "  " << key << " = " << config.settings().get(key)
                     << "\n";
                return HistoryStatus::Success;
            }

            case ConfigCommand::Action::Set: {
                const std::string& key   = cmd.key();
                const std::string& value = cmd.value();

                if (key.empty() || value.empty()) {
                    diag::emit_missing_config_kv(base);
                    return HistoryStatus::Error;
                }
                if (!Settings::is_valid_key(key)) {
                    diag::emit_unknown_setting(base, key);
                    return HistoryStatus::Error;
                }

                if (key == "auto_load_env" && !value.empty() &&
                    !config.env_exists(value)) {
                    diag::emit_env_ref_error(base, value, config);
                    return HistoryStatus::Error;
                }

                std::string err_msg = config.settings().set(key, value);
                if (!err_msg.empty()) {
                    diag::emit_invalid_setting_value(base, key, value, err_msg);
                    return HistoryStatus::Error;
                }

                config.save();
                cout << "  " << key << " = " << config.settings().get(key)
                     << "\n";
                return HistoryStatus::Success;
            }

            case ConfigCommand::Action::Path:
                cout << "  " << config.file_path() << "\n";
                return HistoryStatus::Info;

            case ConfigCommand::Action::Reset:
                config.reset_settings();
                config.save();
                cout << "  Settings reset to defaults\n";
                return HistoryStatus::Success;

            case ConfigCommand::Action::Unknown:
                diag::emit_unknown_config_subcommand(base, cmd.key());
                return HistoryStatus::Error;
            }

            return HistoryStatus::Unknown;
        }

    } // namespace handlers
} // namespace math_solver