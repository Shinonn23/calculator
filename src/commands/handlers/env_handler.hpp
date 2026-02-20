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

        // Persists the current Context's variable state into the named
        // environment.
        // - Assumes ctx is in a valid, consistent state.
        // - Used to ensure that switching environments does not lose local
        // state.
        inline void save_current_env(Config&            config,
                                     const std::string& env_name,
                                     const Context&     ctx) {
            config.save_env_variables(env_name, ctx.all_as_strings());
        }

        // Loads variables from the specified environment into ctx.
        // - Returns false if the environment does not exist (diagnostics
        // emitted).
        // - Always clears ctx before population to avoid stale state.
        // - Parsing failures for individual variables are ignored (legacy:
        // avoids
        //   hard errors on corrupt or legacy entries).
        // - Invariant: ctx is always left in a cleared+repopulated state on
        // success.
        // - This function is performance-sensitive for large environments.
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
                // Attempt to parse as expression; fallback to double if parse
                // fails. Malformed entries are skipped to avoid breaking the
                // environment.
                try {
                    Parser parser(expr_str);
                    ctx.set(name, parser.parse());
                } catch (...) {
                    try {
                        ctx.set(name, std::stod(expr_str));
                    } catch (...) {
                        // Ignore malformed entries.
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
        // - All mutations to environments must go through this entrypoint to
        // avoid
        //   violating invariants.
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

            case EnvCommand::Action::Move: {
                const auto& flags = cmd.flags();

                if (flags.vars_mode) {
                    // Moves a subset of variables from the current environment
                    // to the target, removing them from the current environment
                    // after transfer.
                    // - Invariant: current_env remains valid after mutation.
                    // - If a variable is missing, emits a warning but
                    // continues.
                    const std::string& dest = flags.to_env;
                    if (dest.empty()) {
                        std::cout << "  Usage: :env mv --vars x y --to "
                                     "<target_env>\n";
                        break;
                    }
                    if (!config.env_exists(dest)) {
                        std::cout << ansi::red << "  Error: " << ansi::reset
                                  << "environment '" << dest << "' not found\n";
                        maybe_suggest(dest, config.list_envs());
                        break;
                    }

                    auto all = ctx.all_as_strings();
                    std::unordered_map<std::string, std::string> subset;

                    for (const auto& v : cmd.vars_to_save()) {
                        if (auto it = all.find(v); it != all.end()) {
                            subset[v] = it->second;
                            ctx.unset(v); // Remove from current env after move.
                        } else {
                            std::cout << ansi::yellow
                                      << "  Warning: " << ansi::reset
                                      << "variable '" << v
                                      << "' not defined, skipped\n";
                        }
                    }

                    config.save_env_variables(dest, subset);
                    // Sync current env after mutation to maintain persistence
                    // invariants.
                    save_current_env(config, current_env, ctx);
                    std::cout << "  Moved " << subset.size()
                              << " variable(s) to '" << ansi::bold << dest
                              << ansi::reset << "'\n";

                } else {
                    // Moves (renames) an entire environment.
                    // - Disallowed for the currently active environment to
                    // avoid
                    //   invalidating current_env invariants.
                    // - If dest exists, it is overwritten.
                    // - Source is deleted after copy.
                    const std::string& src  = cmd.source_env();
                    const std::string& dest = cmd.target_env();

                    if (src.empty() || dest.empty()) {
                        std::cout
                            << "  Usage: :env mv <source_env> <target_env>\n";
                        break;
                    }
                    if (src == current_env) {
                        std::cout << ansi::red << "  Error: " << ansi::reset
                                  << "cannot move the active environment\n";
                        break;
                    }
                    if (!config.env_exists(src)) {
                        std::cout << ansi::red << "  Error: " << ansi::reset
                                  << "environment '" << src << "' not found\n";
                        maybe_suggest(src, config.list_envs());
                        break;
                    }

                    try {
                        config.create_env(dest);
                        config.save_env_variables(
                            dest, config.get_env(src).variables);
                        config.delete_env(src);
                        std::cout << "  Moved environment '" << src << "' → '"
                                  << ansi::bold << dest << ansi::reset << "'\n";
                    } catch (const std::exception& e) {
                        std::cout << ansi::red << "  Error: " << ansi::reset
                                  << e.what() << "\n";
                    }
                }
                break;
            }

            case EnvCommand::Action::Copy: {
                const auto& flags = cmd.flags();

                if (flags.vars_mode) {
                    // Copies a subset of variables from the current environment
                    // to the target.
                    // - Does not mutate the current environment.
                    // - If a variable is missing, emits a warning but
                    // continues.
                    const std::string& dest = flags.to_env;
                    if (dest.empty()) {
                        std::cout << "  Usage: :env cp --vars x y --to "
                                     "<target_env>\n";
                        break;
                    }
                    if (!config.env_exists(dest)) {
                        std::cout << ansi::red << "  Error: " << ansi::reset
                                  << "environment '" << dest << "' not found\n";
                        maybe_suggest(dest, config.list_envs());
                        break;
                    }

                    auto all = ctx.all_as_strings();
                    std::unordered_map<std::string, std::string> subset;

                    for (const auto& v : cmd.vars_to_save()) {
                        if (auto it = all.find(v); it != all.end())
                            subset[v] = it->second;
                        else
                            std::cout << ansi::yellow
                                      << "  Warning: " << ansi::reset
                                      << "variable '" << v
                                      << "' not defined, skipped\n";
                    }

                    config.save_env_variables(dest, subset);
                    std::cout << "  Copied " << subset.size()
                              << " variable(s) to '" << ansi::bold << dest
                              << ansi::reset << "'\n";

                } else {
                    // Copies an entire environment to a new name.
                    // - If dest exists, it is overwritten.
                    // - Source is not mutated.
                    const std::string& src  = cmd.source_env();
                    const std::string& dest = cmd.target_env();

                    if (src.empty() || dest.empty()) {
                        std::cout
                            << "  Usage: :env cp <source_env> <target_env>\n";
                        break;
                    }
                    if (!config.env_exists(src)) {
                        std::cout << ansi::red << "  Error: " << ansi::reset
                                  << "environment '" << src << "' not found\n";
                        maybe_suggest(src, config.list_envs());
                        break;
                    }

                    try {
                        config.create_env(dest);
                        config.save_env_variables(
                            dest, config.get_env(src).variables);
                        std::cout << "  Copied environment '" << src << "' → '"
                                  << ansi::bold << dest << ansi::reset << "'\n";
                    } catch (const std::exception& e) {
                        std::cout << ansi::red << "  Error: " << ansi::reset
                                  << e.what() << "\n";
                    }
                }
                break;
            }
            }
        }

    } // namespace handlers
} // namespace math_solver
