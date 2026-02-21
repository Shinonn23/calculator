#pragma once

#include "ast/command/env_command.hpp"
#include "ast/command/history_entry.hpp"
#include "config/config.hpp"
#include "core/error.hpp"
#include "parser/math/math_parser.hpp"
#include "runtime/context/context.hpp"
#include "ui/color.hpp"
#include "ui/suggestions.hpp"

#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

namespace math_solver {
    namespace handlers {

        // Returns the span of the first occurrence of `token` in `raw`.
        // Used for error reporting; assumes tokens are not repeated or
        // ambiguous in the input.
        inline Span find_env_token_span(const std::string& raw,
                                        const std::string& token) {
            if (token.empty())
                return Span();
            size_t pos = raw.find(token);
            if (pos != std::string::npos) {
                return Span(pos, pos + token.length());
            }
            return Span();
        }

        // Persists the current context's variables into the named environment.
        // This is called before switching environments to avoid state loss.
        inline void save_current_env(Config&            config,
                                     const std::string& env_name,
                                     const Context&     ctx) {
            config.save_env_variables(env_name, ctx.all_as_strings());
        }

        // Loads variables from the named environment into the given context.
        // Returns false and emits diagnostics if the environment does not
        // exist. Parsing failures for individual variables are tolerated
        // (fallback to double). Invariant: context is cleared before loading.
        inline bool load_env_into_context(Config&            config,
                                          const std::string& env_name,
                                          Context&           ctx,
                                          const std::string& raw) {
            if (!config.env_exists(env_name)) {
                MathError err("environment `" + env_name + "` not found",
                              find_env_token_span(raw, env_name),
                              raw);
                err.with_code("E0601").with_label("unknown environment");

                auto match = suggest(env_name, config.list_envs());
                if (match) {
                    err.with_help(
                        "an environment with a similar name exists: `" +
                        *match + "`");
                }
                std::cout << err.format();
                return false;
            }

            ctx.clear();
            const auto& env = config.get_env(env_name);
            for (const auto& [name, expr_str] : env.variables) {
                // Parsing failures are not fatal; fallback to numeric parsing.
                try {
                    Parser parser(expr_str);
                    ctx.set(name, parser.parse());
                } catch (...) {
                    try {
                        ctx.set(name, std::stod(expr_str));
                    } catch (...) {
                    }
                }
            }
            return true;
        }

        // Handles all environment-related commands.
        // Maintains the invariant that the current environment is always
        // persisted before switching. All error reporting is routed through
        // MathError for consistency. Returns a HistoryStatus indicating the
        // outcome for integration with the REPL history.
        inline HistoryStatus handle_env(const EnvCommand& cmd,
                                        Context&          ctx,
                                        Config&           config,
                                        std::string&      current_env) {
            const std::string& raw = cmd.raw_command();

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
                    MathError err("missing environment name",
                                  find_env_token_span(raw, "load"),
                                  raw);
                    err.with_code("E0600").with_help(
                        "Usage: `:env load <name>`");
                    std::cout << err.format();
                    return HistoryStatus::Error;
                }
                // Always persist current environment before switching.
                save_current_env(config, current_env, ctx);
                if (load_env_into_context(config, target, ctx, raw)) {
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

                // If a variable list is provided, only those are saved.
                // Variables not present in the context are skipped with a
                // warning.
                if (!vars_to_save.empty()) {
                    std::unordered_map<std::string, std::string> subset;
                    auto all = ctx.all_as_strings();
                    for (const auto& v : vars_to_save) {
                        if (auto it = all.find(v); it != all.end()) {
                            subset[v] = it->second;
                        } else {
                            std::cout << ansi::yellow
                                      << "  Warning: " << ansi::reset
                                      << "variable '" << v
                                      << "' not defined, skipped\n";
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
                    MathError err("missing environment name",
                                  find_env_token_span(raw, "new"),
                                  raw);
                    err.with_code("E0600").with_help(
                        "Usage: `:env new <name>`");
                    std::cout << err.format();
                    return HistoryStatus::Error;
                }
                // Creating an environment may fail if the name is already
                // taken.
                try {
                    config.create_env(name);
                    std::cout << "  Created environment '" << ansi::bold << name
                              << ansi::reset << "'\n";
                    return HistoryStatus::Success;
                } catch (const std::exception& e) {
                    MathError err(
                        e.what(), find_env_token_span(raw, name), raw);
                    err.with_code("E0604");
                    std::cout << err.format();
                    return HistoryStatus::Error;
                }
            }

            case EnvCommand::Action::Delete: {
                const std::string& name = cmd.target_env();
                if (name.empty()) {
                    MathError err("missing environment name",
                                  find_env_token_span(raw, "delete"),
                                  raw);
                    err.with_code("E0600").with_help(
                        "Usage: `:env delete <name>`");
                    std::cout << err.format();
                    return HistoryStatus::Error;
                }
                // Prevent deletion of the active environment to avoid undefined
                // state.
                if (name == current_env) {
                    MathError err("cannot delete the active environment",
                                  find_env_token_span(raw, name),
                                  raw);
                    err.with_code("E0602")
                        .with_label("active environment")
                        .with_help("switch to another environment using `:env "
                                   "load <name>` first.");
                    std::cout << err.format();
                    return HistoryStatus::Error;
                }
                try {
                    config.delete_env(name);
                    std::cout << "  Deleted environment '" << name << "'\n";
                    return HistoryStatus::Success;
                } catch (const std::exception& e) {
                    MathError err(
                        e.what(), find_env_token_span(raw, name), raw);
                    err.with_code("E0604");
                    std::cout << err.format();
                    return HistoryStatus::Error;
                }
            }

            case EnvCommand::Action::Move: {
                const auto& flags = cmd.flags();

                if (flags.vars_mode) {
                    // Moves a subset of variables from the current context to
                    // another environment. Variables are removed from the
                    // current context after transfer.
                    const std::string& dest = flags.to_env;
                    if (dest.empty()) {
                        MathError err("missing target environment",
                                      find_env_token_span(raw, "--to"),
                                      raw);
                        err.with_code("E0600").with_help(
                            "Usage: `:env mv --vars x y --to <target_env>`");
                        std::cout << err.format();
                        return HistoryStatus::Error;
                    }
                    if (!config.env_exists(dest)) {
                        MathError err("environment `" + dest + "` not found",
                                      find_env_token_span(raw, dest),
                                      raw);
                        err.with_code("E0601").with_label(
                            "unknown environment");
                        auto match = suggest(dest, config.list_envs());
                        if (match)
                            err.with_help(
                                "an environment with a similar name exists: `" +
                                *match + "`");
                        std::cout << err.format();
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
                            std::cout << ansi::yellow
                                      << "  Warning: " << ansi::reset
                                      << "variable '" << v
                                      << "' not defined, skipped\n";
                            has_warning = true;
                        }
                    }

                    config.save_env_variables(dest, subset);
                    // Persist the updated context after variable removal.
                    save_current_env(config, current_env, ctx);
                    std::cout << "  Moved " << subset.size()
                              << " variable(s) to '" << ansi::bold << dest
                              << ansi::reset << "'\n";
                    return has_warning ? HistoryStatus::Warning
                                       : HistoryStatus::Success;

                } else {
                    // Moves an entire environment to a new name.
                    // Disallows moving the active environment to avoid state
                    // corruption.
                    const std::string& src  = cmd.source_env();
                    const std::string& dest = cmd.target_env();

                    if (src.empty() || dest.empty()) {
                        MathError err("missing source or target environment",
                                      find_env_token_span(raw, "mv"),
                                      raw);
                        err.with_code("E0600").with_help(
                            "Usage: `:env mv <source_env> <target_env>`");
                        std::cout << err.format();
                        return HistoryStatus::Error;
                    }
                    if (src == current_env) {
                        MathError err("cannot move the active environment",
                                      find_env_token_span(raw, src),
                                      raw);
                        err.with_code("E0603").with_label("active environment");
                        std::cout << err.format();
                        return HistoryStatus::Error;
                    }
                    if (!config.env_exists(src)) {
                        MathError err("environment `" + src + "` not found",
                                      find_env_token_span(raw, src),
                                      raw);
                        err.with_code("E0601").with_label(
                            "unknown environment");
                        auto match = suggest(src, config.list_envs());
                        if (match)
                            err.with_help(
                                "an environment with a similar name exists: `" +
                                *match + "`");
                        std::cout << err.format();
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
                        MathError err(
                            e.what(), find_env_token_span(raw, dest), raw);
                        err.with_code("E0604");
                        std::cout << err.format();
                        return HistoryStatus::Error;
                    }
                }
            }

            case EnvCommand::Action::Copy: {
                const auto& flags = cmd.flags();

                if (flags.vars_mode) {
                    // Copies a subset of variables from the current context to
                    // another environment. Unlike move, variables remain in the
                    // current context.
                    const std::string& dest = flags.to_env;
                    if (dest.empty()) {
                        MathError err("missing target environment",
                                      find_env_token_span(raw, "--to"),
                                      raw);
                        err.with_code("E0600").with_help(
                            "Usage: `:env cp --vars x y --to <target_env>`");
                        std::cout << err.format();
                        return HistoryStatus::Error;
                    }
                    if (!config.env_exists(dest)) {
                        MathError err("environment `" + dest + "` not found",
                                      find_env_token_span(raw, dest),
                                      raw);
                        err.with_code("E0601").with_label(
                            "unknown environment");
                        auto match = suggest(dest, config.list_envs());
                        if (match)
                            err.with_help(
                                "an environment with a similar name exists: `" +
                                *match + "`");
                        std::cout << err.format();
                        return HistoryStatus::Error;
                    }

                    auto all = ctx.all_as_strings();
                    std::unordered_map<std::string, std::string> subset;
                    bool has_warning = false;

                    for (const auto& v : cmd.vars_to_save()) {
                        if (auto it = all.find(v); it != all.end()) {
                            subset[v] = it->second;
                        } else {
                            std::cout << ansi::yellow
                                      << "  Warning: " << ansi::reset
                                      << "variable '" << v
                                      << "' not defined, skipped\n";
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
                    // Copies an entire environment to a new name.
                    // No restrictions on copying the active environment.
                    const std::string& src  = cmd.source_env();
                    const std::string& dest = cmd.target_env();

                    if (src.empty() || dest.empty()) {
                        MathError err("missing source or target environment",
                                      find_env_token_span(raw, "cp"),
                                      raw);
                        err.with_code("E0600").with_help(
                            "Usage: `:env cp <source_env> <target_env>`");
                        std::cout << err.format();
                        return HistoryStatus::Error;
                    }
                    if (!config.env_exists(src)) {
                        MathError err("environment `" + src + "` not found",
                                      find_env_token_span(raw, src),
                                      raw);
                        err.with_code("E0601").with_label(
                            "unknown environment");
                        auto match = suggest(src, config.list_envs());
                        if (match)
                            err.with_help(
                                "an environment with a similar name exists: `" +
                                *match + "`");
                        std::cout << err.format();
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
                        MathError err(
                            e.what(), find_env_token_span(raw, dest), raw);
                        err.with_code("E0604");
                        std::cout << err.format();
                        return HistoryStatus::Error;
                    }
                }
            }
            }
            return HistoryStatus::Unknown;
        }

    } // namespace handlers
} // namespace math_solver