#pragma once

#include "ast/command/env_command.hpp"
#include "ast/command/history_entry.hpp"
#include "commands/handlers/diagnostics/env_diag.hpp"
#include "config/config.hpp"
#include "core/diagnostic_sink.hpp"
#include "core/error.hpp"
#include "parser/math/math_parser.hpp"
#include "runtime/context/context.hpp"
#include "ui/color.hpp"

#include <iostream>
#include <unordered_map>

namespace math_solver {
    namespace handlers {

        inline void save_current_env(Config&            config,
                                     const std::string& env_name,
                                     const Context&     ctx) {
            config.save_env_variables(env_name, ctx.all_as_strings());
        }

        inline bool load_env_into_context(Config&            config,
                                          const std::string& env_name,
                                          Context& ctx, const std::string& raw,
                                          size_t             file_line,
                                          const std::string& filename) {
            if (!config.env_exists(env_name)) {
                DiagnosticBuilder base;
                base.input     = raw;
                base.file_line = file_line;
                base.filename  = filename;
                std::cout
                    << diag::env_not_found(base, env_name, config).build();
                return false;
            }
            ctx.clear();
            const auto& env = config.get_env(env_name);
            for (const auto& [name, expr_str] : env.variables) {
                try {
                    Parser parser(expr_str);
                    auto   parse_result = parser.parse();
                    if (!parse_result)
                        throw std::runtime_error("parse failed");
                    ctx.set(name, std::move(*parse_result));
                } catch (...) {
                    try {
                        ctx.set(name, std::stod(expr_str));
                    } catch (...) {
                    }
                }
            }
            return true;
        }

        inline HistoryStatus handle_env(const EnvCommand& cmd, Context& ctx,
                                        Config&      config,
                                        std::string& current_env,
                                        DiagnosticSink& /*sink*/) {
            const std::string& raw  = cmd.raw_command();
            auto               base = diag::from_cmd(cmd);

            switch (cmd.action()) {

            case EnvCommand::Action::Show:
                std::cout << "  Current environment: " << ansi::bold
                          << current_env << ansi::reset << "\n";
                return HistoryStatus::Info;

            case EnvCommand::Action::List: {
                auto envs = config.list_envs();
                if (envs.empty()) {
                    std::cout << "  No environments defined\n";
                } else {
                    for (const auto& name : envs) {
                        if (name == current_env)
                            std::cout << "  * " << ansi::bold << name
                                      << ansi::reset << " (current)\n";
                        else
                            std::cout << "    " << name << "\n";
                    }
                }
                return HistoryStatus::Info;
            }

            case EnvCommand::Action::Load: {
                const std::string& target = cmd.target_env();
                if (target.empty()) {
                    std::cout
                        << diag::missing_name(base, "load", "`env load <name>`")
                               .build();
                    return HistoryStatus::Error;
                }
                save_current_env(config, current_env, ctx);
                if (load_env_into_context(config, target, ctx, raw,
                                          cmd.source_line(),
                                          cmd.source_file())) {
                    current_env = target;
                    std::cout << "  Switched to environment '" << ansi::bold
                              << target << ansi::reset << "'\n";
                    return HistoryStatus::Success;
                }
                return HistoryStatus::Error;
            }

            case EnvCommand::Action::Save: {
                const std::string& target = cmd.target_env();
                const std::string& dest = target.empty() ? current_env : target;
                const auto&        vars_to_save = cmd.vars_to_save();
                bool               has_warning  = false;

                if (!vars_to_save.empty()) {
                    std::unordered_map<std::string, std::string> subset;
                    auto all = ctx.all_as_strings();
                    for (const auto& v : vars_to_save) {
                        if (auto it = all.find(v); it != all.end()) {
                            subset[v] = it->second;
                        } else {
                            std::cout << diag::var_skipped(base, v).build();
                            has_warning = true;
                        }
                    }
                    config.save_env_variables(dest, subset);
                } else {
                    save_current_env(config, dest, ctx);
                }
                std::cout << "  Saved to environment '" << ansi::bold << dest
                          << ansi::reset << "'\n";
                return has_warning ? HistoryStatus::Warning
                                   : HistoryStatus::Success;
            }

            case EnvCommand::Action::New: {
                const std::string& name = cmd.target_env();
                if (name.empty()) {
                    std::cout
                        << diag::missing_name(base, "new", "`env new <name>`")
                               .build();
                    return HistoryStatus::Error;
                }
                try {
                    config.create_env(name);
                    std::cout << "  Created environment '" << ansi::bold << name
                              << ansi::reset << "'\n";
                    return HistoryStatus::Success;
                } catch (const std::exception& e) {
                    std::cout
                        << diag::runtime_error(base, e.what(), name, "E0604")
                               .build();
                    return HistoryStatus::Error;
                }
            }

            case EnvCommand::Action::Delete: {
                const std::string& name = cmd.target_env();
                if (name.empty()) {
                    std::cout << diag::missing_name(base, "delete",
                                                    "`env delete <name>`")
                                     .build();
                    return HistoryStatus::Error;
                }
                if (name == current_env) {
                    std::cout
                        << diag::active_env_protected(base, name, "delete")
                               .build();
                    return HistoryStatus::Error;
                }
                try {
                    config.delete_env(name);
                    std::cout << "  Deleted environment '" << name << "'\n";
                    return HistoryStatus::Success;
                } catch (const std::exception& e) {
                    std::cout
                        << diag::runtime_error(base, e.what(), name, "E0604")
                               .build();
                    return HistoryStatus::Error;
                }
            }

            case EnvCommand::Action::Move: {
                const auto& flags = cmd.flags();
                if (flags.vars_mode) {
                    const std::string& dest = flags.to_env;
                    if (dest.empty()) {
                        std::cout << diag::missing_name(
                                         base, "--to",
                                         "`env mv --vars x y --to <env>`")
                                         .build();
                        return HistoryStatus::Error;
                    }
                    if (!config.env_exists(dest)) {
                        std::cout
                            << diag::env_not_found(base, dest, config).build();
                        return HistoryStatus::Error;
                    }
                    auto all = ctx.all_as_strings();
                    std::unordered_map<std::string, std::string> subset;
                    bool has_warning = false;
                    for (const auto& v : cmd.vars_to_save()) {
                        if (auto it = all.find(v); it != all.end()) {
                            subset[v] = it->second;
                            ctx.unset(v);
                        } else {
                            std::cout << diag::var_skipped(base, v).build();
                            has_warning = true;
                        }
                    }
                    config.save_env_variables(dest, subset);
                    save_current_env(config, current_env, ctx);
                    std::cout << "  Moved " << subset.size()
                              << " variable(s) to '" << ansi::bold << dest
                              << ansi::reset << "'\n";
                    return has_warning ? HistoryStatus::Warning
                                       : HistoryStatus::Success;
                } else {
                    const std::string& src  = cmd.source_env();
                    const std::string& dest = cmd.target_env();
                    if (src.empty() || dest.empty()) {
                        std::cout << diag::missing_name(base, "mv",
                                                        "`env mv <src> <dst>`")
                                         .build();
                        return HistoryStatus::Error;
                    }
                    if (src == current_env) {
                        std::cout
                            << diag::active_env_protected(base, src, "move")
                                   .build();
                        return HistoryStatus::Error;
                    }
                    if (!config.env_exists(src)) {
                        std::cout
                            << diag::env_not_found(base, src, config).build();
                        return HistoryStatus::Error;
                    }
                    try {
                        config.create_env(dest);
                        config.save_env_variables(
                            dest, config.get_env(src).variables);
                        config.delete_env(src);
                        std::cout << "  Moved environment '" << src << "' → '"
                                  << ansi::bold << dest << ansi::reset << "'\n";
                        return HistoryStatus::Success;
                    } catch (const std::exception& e) {
                        std::cout << diag::runtime_error(base, e.what(), dest,
                                                         "E0604")
                                         .build();
                        return HistoryStatus::Error;
                    }
                }
            }

            case EnvCommand::Action::Copy: {
                const auto& flags = cmd.flags();
                if (flags.vars_mode) {
                    const std::string& dest = flags.to_env;
                    if (dest.empty()) {
                        std::cout << diag::missing_name(
                                         base, "--to",
                                         "`env cp --vars x y --to <env>`")
                                         .build();
                        return HistoryStatus::Error;
                    }
                    if (!config.env_exists(dest)) {
                        std::cout
                            << diag::env_not_found(base, dest, config).build();
                        return HistoryStatus::Error;
                    }
                    auto all = ctx.all_as_strings();
                    std::unordered_map<std::string, std::string> subset;
                    bool has_warning = false;
                    for (const auto& v : cmd.vars_to_save()) {
                        if (auto it = all.find(v); it != all.end()) {
                            subset[v] = it->second;
                        } else {
                            std::cout << diag::var_skipped(base, v).build();
                            has_warning = true;
                        }
                    }
                    config.save_env_variables(dest, subset);
                    std::cout << "  Copied " << subset.size()
                              << " variable(s) to '" << ansi::bold << dest
                              << ansi::reset << "'\n";
                    return has_warning ? HistoryStatus::Warning
                                       : HistoryStatus::Success;
                } else {
                    const std::string& src  = cmd.source_env();
                    const std::string& dest = cmd.target_env();
                    if (src.empty() || dest.empty()) {
                        std::cout << diag::missing_name(base, "cp",
                                                        "`env cp <src> <dst>`")
                                         .build();
                        return HistoryStatus::Error;
                    }
                    if (!config.env_exists(src)) {
                        std::cout
                            << diag::env_not_found(base, src, config).build();
                        return HistoryStatus::Error;
                    }
                    try {
                        config.create_env(dest);
                        config.save_env_variables(
                            dest, config.get_env(src).variables);
                        std::cout << "  Copied environment '" << src << "' → '"
                                  << ansi::bold << dest << ansi::reset << "'\n";
                        return HistoryStatus::Success;
                    } catch (const std::exception& e) {
                        std::cout << diag::runtime_error(base, e.what(), dest,
                                                         "E0604")
                                         .build();
                        return HistoryStatus::Error;
                    }
                }
            }

            case EnvCommand::Action::Unknown: {
                const std::string&                    sub  = cmd.raw_command();
                static const std::vector<std::string> subs = {
                    "show", "list",   "load", "save",
                    "new",  "delete", "mv",   "cp"};
                std::cout << diag::unknown_subcommand(base, sub, subs).build();
                return HistoryStatus::Error;
            }
            }
            return HistoryStatus::Unknown;
        }

    } // namespace handlers
} // namespace math_solver