#ifndef REPLXX_H
#define REPLXX_H
#include "command.hpp"
#include "config.hpp"
#include <replxx.hxx>
#include <string>
#include <vector>

using namespace std;

namespace math_solver {
    inline void setup_completions(replxx::Replxx& rx, Config& g_config,
                                  Context& g_ctx) {
        rx.set_completion_callback([&g_config, &g_ctx](const string& input,
                                                       int&          contextLen)
                                       -> replxx::Replxx::completions_t {
            replxx::Replxx::completions_t completions;

            // Trim leading whitespace
            string                        trimmed = input;
            size_t                        s = trimmed.find_first_not_of(" \t");
            if (s == string::npos)
                trimmed.clear();
            else
                trimmed = trimmed.substr(s);

            // Completing first word
            if (trimmed.find(' ') == string::npos) {
                contextLen = static_cast<int>(trimmed.size());
                for (const auto& cmd : ALL_COMMANDS) {
                    if (starts_with(cmd, trimmed))
                        completions.emplace_back(cmd);
                }
                return completions;
            }

            // Multi-word: find last word being typed
            size_t last_space = trimmed.find_last_of(' ');
            string last_word  = (last_space != string::npos)
                                    ? trimmed.substr(last_space + 1)
                                    : trimmed;
            contextLen        = static_cast<int>(last_word.size());

            // :config subcommands
            if (starts_with(trimmed, ":config ")) {
                vector<string> parts = split(trimmed);
                if (parts.size() <= 2) {
                    for (const auto& sub : CONFIG_SUBCOMMANDS) {
                        if (last_word.empty() || starts_with(sub, last_word))
                            completions.emplace_back(sub);
                    }
                } else if (parts.size() >= 2 &&
                           (parts[1] == "get" || parts[1] == "set")) {
                    for (const auto& key : Settings::all_keys()) {
                        if (last_word.empty() || starts_with(key, last_word))
                            completions.emplace_back(key);
                    }
                }
                return completions;
            }

            // :env subcommands
            if (starts_with(trimmed, ":env ")) {
                vector<string> parts = split(trimmed);
                if (parts.size() <= 2) {
                    for (const auto& sub : ENV_SUBCOMMANDS) {
                        if (last_word.empty() || starts_with(sub, last_word))
                            completions.emplace_back(sub);
                    }
                } else if (parts.size() >= 2 &&
                           (parts[1] == "load" || parts[1] == "delete")) {
                    for (const auto& name : g_config.list_envs()) {
                        if (last_word.empty() || starts_with(name, last_word))
                            completions.emplace_back(name);
                    }
                } else if (parts.size() >= 2 && parts[1] == "save") {
                    // After :env save <name>, complete with variable names
                    if (parts.size() == 2) {
                        for (const auto& name : g_config.list_envs()) {
                            if (last_word.empty() ||
                                starts_with(name, last_word))
                                completions.emplace_back(name);
                        }
                    } else {
                        for (const auto& name : g_ctx.all_names()) {
                            if (last_word.empty() ||
                                starts_with(name, last_word))
                                completions.emplace_back(name);
                        }
                    }
                }
                return completions;
            }

            // :set — complete variable names
            if (starts_with(trimmed, ":set ")) {
                vector<string> parts = split(trimmed);
                if (parts.size() <= 2) {
                    for (const auto& name : g_ctx.all_names()) {
                        if (last_word.empty() || starts_with(name, last_word))
                            completions.emplace_back(name);
                    }
                }
                return completions;
            }

            // :unset — complete variable names
            if (starts_with(trimmed, ":unset ")) {
                for (const auto& name : g_ctx.all_names()) {
                    if (last_word.empty() || starts_with(name, last_word))
                        completions.emplace_back(name);
                }
                return completions;
            }

            // simplify flags
            if (starts_with(trimmed, "simplify ") &&
                starts_with(last_word, "--")) {
                vector<string> flags = {"--vars", "--isolated", "--fraction"};
                for (const auto& f : flags) {
                    if (starts_with(f, last_word))
                        completions.emplace_back(f);
                }
                return completions;
            }

            return completions;
        });

        rx.set_word_break_characters(" \t");
    }

    inline void setup_hints(replxx::Replxx& rx) {
        rx.set_hint_callback(
            [](const string& input, int& contextLen,
               replxx::Replxx::Color& color) -> replxx::Replxx::hints_t {
                replxx::Replxx::hints_t hints;
                color          = replxx::Replxx::Color::GRAY;

                string trimmed = input;
                size_t s       = trimmed.find_first_not_of(" \t");
                if (s == string::npos)
                    return hints;
                trimmed    = trimmed.substr(s);
                contextLen = 0;

                // Single-word hints
                if (trimmed.find(' ') == string::npos) {
                    if (trimmed == ":set")
                        hints.emplace_back(" <variable> <value | solve <eq>>");
                    else if (trimmed == ":unset")
                        hints.emplace_back(" <variable>");
                    else if (trimmed == "solve")
                        hints.emplace_back(" <lhs> = <rhs>");
                    else if (trimmed == "simplify")
                        hints.emplace_back(
                            " <lhs> = <rhs> [--vars x y] [--isolated] "
                            "[--fraction]");
                    else if (trimmed == ":config")
                        hints.emplace_back(" <list|get|set|path|reset>");
                    else if (trimmed == ":env")
                        hints.emplace_back(" [list|load|save|new|delete]");
                }

                return hints;
            });

        rx.set_hint_delay(300);
    }

} // namespace math_solver

#endif