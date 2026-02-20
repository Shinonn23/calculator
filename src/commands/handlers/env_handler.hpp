#pragma once

#include "ast/command/env_command.hpp"
#include "config/config.hpp"
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
        // Persists the current context's variables to the named environment.
        // Assumes ctx is in a consistent state and env_name is valid for
        // storage.
        inline void save_current_env(Config&            config,
                                     const std::string& env_name,
                                     const Context&     ctx) {
            config.save_env_variables(env_name, ctx.all_as_strings());
        }

        // Loads variables from the named environment into the given context.
        // - If the environment does not exist, emits diagnostics and returns
        // false.
        // - On success, clears ctx and repopulates it from the environment.
        // - Parsing failures are silently skipped (legacy: avoids hard errors
        // on corrupt entries).
        // - Invariant: ctx is always cleared before population to avoid stale
        // state.
        inline bool load_env_into_context(Config&            config,
                                          const std::string& env_name,
                                          Context&           ctx) {
            if (!config.env_exists(env_name)) {
                std::cout << ansi::red << "  Error: " << ansi::reset
                          << "environment '" << env_name << "' not found\n";
                maybe_suggest(env_name, config.list_envs());
                return false;
            }

            ctx.clear();
            const auto& env = config.get_env(env_name);
            for (const auto& [name, expr_str] : env.variables) {
                try {
                    Parser parser(expr_str);
                    ctx.set(name, parser.parse());
                } catch (...) {
                    try {
                        ctx.set(name, std::stod(expr_str));
                    } catch (...) {
                        // Intentionally ignore malformed entries; see above.
                    }
                }
            }
            return true;
        }

        // Handles all environment-related commands.
        // - Maintains the invariant that current_env always reflects the active
        // context.
        // - Persists changes before switching environments to avoid data loss.
        // - Defensive against invalid or missing environment names.
        // - Interacts with Config for all persistent state; Context is always
        // local.
        inline void handle_env(const EnvCommand& cmd,
                               Context&          ctx,
                               Config&           config,
                               std::string&      current_env) {
            switch (cmd.action()) {
            case EnvCommand::Action::Show:
                std::cout << "  Current environment: " << ansi::bold
                          << current_env << ansi::reset << "\n";
                break;

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
                break;
            }

            case EnvCommand::Action::Load: {
                const std::string& target = cmd.target_env();
                if (target.empty()) {
                    std::cout << "  Usage: :env load <name>\n";
                    break;
                }
                // Always persist the current environment before switching.
                // This avoids accidental loss of unsaved state.
                save_current_env(config, current_env, ctx);
                if (load_env_into_context(config, target, ctx)) {
                    current_env = target;
                    std::cout << "  Switched to environment '" << ansi::bold
                              << target << ansi::reset << "'\n";
                }
                break;
            }

            case EnvCommand::Action::Save: {
                const std::string& target = cmd.target_env();
                const std::string& dest = target.empty() ? current_env : target;

                const auto&        vars_to_save = cmd.vars_to_save();
                if (!vars_to_save.empty()) {
                    // Only save a subset of variables if explicitly requested.
                    // Warn on missing variables, but do not fail the operation.
                    std::unordered_map<std::string, std::string> subset;
                    auto all = ctx.all_as_strings();
                    for (const auto& v : vars_to_save) {
                        if (auto it = all.find(v); it != all.end())
                            subset[v] = it->second;
                        else
                            std::cout << ansi::yellow
                                      << "  Warning: " << ansi::reset
                                      << "variable '" << v
                                      << "' not defined, skipped\n";
                    }
                    config.save_env_variables(dest, subset);
                } else {
                    save_current_env(config, dest, ctx);
                }
                std::cout << "  Saved to environment '" << ansi::bold << dest
                          << ansi::reset << "'\n";
                break;
            }

            case EnvCommand::Action::New: {
                const std::string& name = cmd.target_env();
                if (name.empty()) {
                    std::cout << "  Usage: :env new <name>\n";
                    break;
                }
                try {
                    // Creating an environment is idempotent if the name is
                    // unique.
                    config.create_env(name);
                    std::cout << "  Created environment '" << ansi::bold << name
                              << ansi::reset << "'\n";
                } catch (const std::exception& e) {
                    std::cout << ansi::red << "  Error: " << ansi::reset
                              << e.what() << "\n";
                }
                break;
            }

            case EnvCommand::Action::Delete: {
                const std::string& name = cmd.target_env();
                if (name.empty()) {
                    std::cout << "  Usage: :env delete <name>\n";
                    break;
                }
                // Prevent deletion of the currently active environment.
                // This avoids invalidating current_env invariants.
                if (name == current_env) {
                    std::cout << ansi::red << "  Error: " << ansi::reset
                              << "cannot delete the active environment\n";
                    break;
                }
                try {
                    config.delete_env(name);
                    std::cout << "  Deleted environment '" << name << "'\n";
                } catch (const std::exception& e) {
                    std::cout << ansi::red << "  Error: " << ansi::reset
                              << e.what() << "\n";
                }
                break;
            }
            }
        }

    } // namespace handlers
} // namespace math_solver
