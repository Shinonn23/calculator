#pragma once

//! # Module — `src/commands/handlers/env_handler.hpp`
//!
//! Implements environment-management command handlers: `save_current_env`,
//! `load_env_into_context`, and the top-level dispatcher `handle_env`.
//! Environments are isolated variable workspaces persisted via `Config`.
//! All functions are `inline` and live entirely in this header.

#include "ast/command/env_command.hpp"
#include "ast/command/history_entry.hpp"
#include "config/config.hpp"
#include "diagnostics/kinds/env_errors.hpp"
#include "parser/math/math_parser.hpp"
#include "runtime/context/context.hpp"
#include "ui/color.hpp"

#include "diagnostics/sink.hpp"
#include <iostream>
#include <sstream>
#include <unordered_map>

namespace math_solver {
    namespace handlers {

        /// Persist all variables in `ctx` to the named environment in `config`.
        ///
        /// Serialises the entire context via `ctx.all_as_strings()` and writes
        /// the result to persistent storage with `config.save_env_variables`.
        ///
        /// # Arguments
        ///
        /// * `config`   — Configuration store that owns the environment data.
        /// * `env_name` — Name of the environment to overwrite.
        /// * `ctx`      — Variable context whose bindings are serialised.
        inline void save_current_env(Config&            config,
                                     const std::string& env_name,
                                     const Context&     ctx) {
            config.save_env_variables(env_name, ctx.all_as_strings());
        }

        /// Clear `ctx` and populate it from the named environment in `config`.
        ///
        /// Retrieves the environment via `config.get_env`. For each stored
        /// variable, attempts to parse the string value as a math expression;
        /// falls back to `std::stod` when parsing fails. Clears `ctx` before
        /// loading — any previous bindings are discarded.
        ///
        /// # Arguments
        ///
        /// * `config`    — Configuration store that provides the environment data.
        /// * `env_name`  — Name of the environment to load.
        /// * `ctx`       — Variable context to populate; cleared before loading.
        /// * `raw`       — Raw command string used for diagnostic span construction.
        /// * `file_line` — Source line number for diagnostics.
        /// * `filename`  — Source file name for diagnostics.
        /// * `sink`      — Diagnostic sink for `env_not_found` errors.
        ///
        /// # Returns
        ///
        /// `true` when the environment is found and loaded successfully;
        /// `false` when the environment does not exist (error pushed to `sink`).
        inline bool load_env_into_context(Config&            config,
                                          const std::string& env_name,
                                          Context& ctx, const std::string& raw,
                                          const Span&        env_span,
                                          size_t             file_line,
                                          const std::string& filename,
                                          DiagnosticSink&    sink) {
            auto env_res = config.get_env(env_name);
            if (!env_res) {
                // If the targeted env isn't found, replace the generic error
                // with a UI-friendly one
                sink.push(errors::env_not_found(raw, env_span, env_name, config,
                                                filename, file_line));
                return false;
            }
            ctx.clear();
            for (const auto& [name, expr_str] : (*env_res)->variables) {
                Parser parser(expr_str);
                auto   parse_result = parser.parse();
                if (parse_result) {
                    ctx.set(name, std::move(*parse_result));
                } else {
                    ctx.set(name, std::stod(expr_str));
                }
            }
            return true;
        }

        /// Dispatch an `EnvCommand` to the appropriate environment sub-handler.
        ///
        /// Routes `cmd.action()` to one of the following sub-operations:
        /// - `Show`   — Emits the active environment name.
        /// - `List`   — Lists all environments, marking the active one with `*`.
        /// - `Load`   — Saves the current env, then loads the target into `ctx`.
        /// - `Save`   — Persists the current context (or a `--vars` subset).
        /// - `New`    — Creates an empty environment via `config.create_env`.
        /// - `Delete` — Removes an environment (active env is protected).
        /// - `Move`   — Renames an env, or moves selected variables to another env.
        /// - `Copy`   — Duplicates an environment.
        /// - `Unknown` — Emits `unknown_env_subcommand` diagnostic.
        ///
        /// # Arguments
        ///
        /// * `cmd`         — The environment command to execute.
        /// * `ctx`         — Variable context; mutated by `Load`, `Save`, `Move`.
        /// * `config`      — Configuration store; mutated by most actions.
        /// * `current_env` — Name of the active environment; updated by `Load`.
        /// * `sink`        — Diagnostic sink for errors, warnings, and output.
        ///
        /// # Returns
        ///
        /// `HistoryStatus::Info` for `Show` and `List`.
        /// `HistoryStatus::Success` for most successful mutating actions.
        /// `HistoryStatus::Warning` when `Save` or `Move` skips missing variables.
        /// `HistoryStatus::Error` on missing names, missing environments, or
        /// attempts to delete/move the active environment.
        ///
        /// # Errors
        ///
        /// Pushes `missing_env_name` when a required name argument is absent.
        /// Pushes `env_not_found` when a referenced environment does not exist.
        /// Pushes E0602 when the active environment is the target of `Delete`/`Move`.
        /// Pushes `var_skipped_warning` (as a warning) for unknown variable names in `--vars`.
        /// Pushes `unknown_env_subcommand` for `Unknown` action.
        inline HistoryStatus handle_env(const EnvCommand& cmd, Context& ctx,
                                        Config&         config,
                                        std::string&    current_env,
                                        DiagnosticSink& sink) {
            const std::string& raw  = cmd.raw_command();
            const std::string& file = cmd.source_file();
            size_t             line = cmd.source_line();

            switch (cmd.action()) {

            case EnvCommand::Action::Show: {
                std::ostringstream oss;
                oss << "  Current environment: " << ansi::bold << current_env
                    << ansi::reset << "\n";
                sink.push_output(oss.str());
                return HistoryStatus::Info;
            }

            case EnvCommand::Action::List: {
                auto envs = config.list_envs();
                if (envs.empty()) {
                    sink.push_output("  No environments defined\n");
                } else {
                    std::ostringstream oss;
                    for (const auto& name : envs) {
                        if (name == current_env)
                            oss << "  * " << ansi::bold << name << ansi::reset
                                << " (current)\n";
                        else
                            oss << "    " << name << "\n";
                    }
                    sink.push_output(oss.str());
                }
                return HistoryStatus::Info;
            }

            case EnvCommand::Action::Load: {
                const std::string& target = cmd.target_env();
                if (target.empty()) {
                    sink.push(errors::missing_env_name(
                        raw, find_token_span(raw, "load"),
                        "`env load <name>`", file, line));
                    return HistoryStatus::Error;
                }
                save_current_env(config, current_env, ctx);
                if (load_env_into_context(config, target, ctx, raw,
                                          cmd.target_span(), cmd.source_line(),
                                          cmd.source_file(), sink)) {
                    current_env = target;
                    std::ostringstream oss;
                    oss << "  Switched to environment '" << ansi::bold << target
                        << ansi::reset << "'\n";
                    sink.push_output(oss.str());
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
                            sink.push(errors::var_skipped_warning(
                                raw, find_token_span(raw, v), v, file, line));
                            has_warning = true;
                        }
                    }
                    config.save_env_variables(dest, subset);
                } else {
                    save_current_env(config, dest, ctx);
                }
                std::ostringstream oss;
                oss << "  Saved to environment '" << ansi::bold << dest
                    << ansi::reset << "'\n";
                sink.push_output(oss.str());
                return has_warning ? HistoryStatus::Warning
                                   : HistoryStatus::Success;
            }

            case EnvCommand::Action::New: {
                const std::string& name = cmd.target_env();
                if (name.empty()) {
                    sink.push(errors::missing_env_name(
                        raw, find_token_span(raw, "new"),
                        "`env new <name>`", file, line));
                    return HistoryStatus::Error;
                }
                auto res = config.create_env(name);
                if (!res) {
                    Diagnostic d = res.error().with_location(file, line);
                    d.span       = cmd.target_span();
                    d.input      = raw;
                    sink.push(d);
                    return HistoryStatus::Error;
                }
                config.save();
                std::ostringstream oss;
                oss << "  Created environment '" << ansi::bold << name
                    << ansi::reset << "'\n";
                sink.push_output(oss.str());
                return HistoryStatus::Success;
            }

            case EnvCommand::Action::Delete: {
                const std::string& name = cmd.target_env();
                if (name.empty()) {
                    sink.push(errors::missing_env_name(
                        raw, find_token_span(raw, "delete"),
                        "`env delete <name>`", file, line));
                    return HistoryStatus::Error;
                }
                if (name == current_env) {
                    Diagnostic d =
                        Diagnostic::make("cannot delete the active environment",
                                         "E0602", cmd.target_span(), raw,
                                         "active environment")
                            .with_location(file, line);
                    d.help = "switch first with `:env load <name>`";
                    sink.push(d);
                    return HistoryStatus::Error;
                }
                auto res = config.delete_env(name);
                if (!res) {
                    Diagnostic d = res.error().with_location(file, line);
                    d.span       = cmd.target_span();
                    d.input      = raw;
                    sink.push(d);
                    return HistoryStatus::Error;
                }
                config.save();
                std::ostringstream oss;
                oss << "  Deleted environment '" << ansi::bold << name
                    << ansi::reset << "'\n";
                sink.push_output(oss.str());
                return HistoryStatus::Success;
            }

            case EnvCommand::Action::Move: {
                const auto& flags = cmd.flags();
                if (flags.vars_mode) {
                    const std::string& dest = flags.to_env;
                    if (dest.empty()) {
                        sink.push(errors::missing_env_name(
                            raw, find_token_span(raw, "--to"),
                            "`env mv --vars x y --to <env>`", file, line));
                        return HistoryStatus::Error;
                    }
                    if (!config.env_exists(dest)) {
                        sink.push(errors::env_not_found(
                            raw, cmd.target_span(), dest, config, file, line));
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
                            sink.push(errors::var_skipped_warning(
                                raw, find_token_span(raw, v), v, file, line));
                            has_warning = true;
                        }
                    }
                    // Merge subset into existing dest env (preserve existing vars)
                    auto dest_res = config.get_env(dest);
                    if (dest_res) {
                        auto merged = (*dest_res)->variables;
                        for (const auto& [k, v] : subset)
                            merged[k] = v;
                        config.save_env_variables(dest, merged);
                    } else {
                        config.save_env_variables(dest, subset);
                    }
                    save_current_env(config, current_env, ctx);
                    std::ostringstream oss;
                    oss << "  Moved " << subset.size() << " variable(s) to '"
                        << ansi::bold << dest << ansi::reset << "'\n";
                    sink.push_output(oss.str());
                    return has_warning ? HistoryStatus::Warning
                                       : HistoryStatus::Success;
                } else {
                    const std::string& src  = cmd.source_env();
                    const std::string& dest = cmd.target_env();
                    if (src.empty() || dest.empty()) {
                        sink.push(errors::missing_env_name(
                            raw, find_token_span(raw, "mv"),
                            "`env mv <src> <dst>`", file, line));
                        return HistoryStatus::Error;
                    }
                    if (src == current_env) {
                        Diagnostic d = Diagnostic::make(
                                           "cannot move the active environment",
                                           "E0602", cmd.source_span(), raw,
                                           "active environment")
                                           .with_location(file, line);
                        d.help = "switch first with `:env load <name>`";
                        sink.push(d);
                        return HistoryStatus::Error;
                    }
                    if (!config.env_exists(src)) {
                        sink.push(errors::env_not_found(
                            raw, cmd.source_span(), src, config, file, line));
                        return HistoryStatus::Error;
                    }
                    config.rename_env(src, dest);
                    std::ostringstream oss;
                    oss << "  Renamed environment '" << ansi::bold << src
                        << ansi::reset << "' to '" << dest << "'\n";
                    sink.push_output(oss.str());
                    return HistoryStatus::Success;
                }
            }

            case EnvCommand::Action::Copy: {
                const std::string& src  = cmd.source_env();
                const std::string& dest = cmd.target_env();
                if (src.empty() || dest.empty()) {
                    sink.push(errors::missing_env_name(
                        raw, find_token_span(raw, "cp"),
                        "`env cp <src> <dst>`", file, line));
                    return HistoryStatus::Error;
                }
                if (!config.env_exists(src)) {
                    sink.push(errors::env_not_found(raw, cmd.source_span(), src,
                                                    config, file, line));
                    return HistoryStatus::Error;
                }
                auto cp_res = config.copy_env(src, dest);
                if (!cp_res) {
                    Diagnostic d = cp_res.error().with_location(file, line);
                    d.span       = cmd.target_span();
                    d.input      = raw;
                    sink.push(d);
                    return HistoryStatus::Error;
                }
                std::ostringstream oss;
                oss << "  Copied environment '" << ansi::bold << src
                    << ansi::reset << "' to '" << dest << "'\n";
                sink.push_output(oss.str());
                return HistoryStatus::Success;
            }

            case EnvCommand::Action::Unknown: {
                const std::string&                    sub  = cmd.raw_command();
                static const std::vector<std::string> subs = {
                    "show", "list",   "load", "save",
                    "new",  "delete", "mv",   "cp"};
                sink.push(
                    errors::unknown_env_subcommand(raw, sub, subs, file, line));
                return HistoryStatus::Error;
            }
            } // end switch
            return HistoryStatus::Unknown;
        }
    } // namespace handlers
} // namespace math_solver