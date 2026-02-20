#pragma once

#include "config/config.hpp"
#include "runtime/context/context.hpp"
#include "utils/string_utils.hpp"

#include <replxx.hxx>
#include <string>
#include <vector>

namespace math_solver {

    // Command lists are intentionally kept as static const to ensure
    // pointer stability and avoid repeated allocations. These are
    // referenced in completion logic and must remain in sync with
    // the REPL's accepted commands.
    inline const std::vector<std::string>& all_commands() {
        static const std::vector<std::string> cmds = {
            ":",
            ":set",
            ":unset",
            ":cls",
            ":clear",
            ":ls",
            ":help",
            ":config",
            ":env",
            ":solve",
            ":simplify",
            ":expand",
            ":factor",
            "exit",
            "quit",
        };
        return cmds;
    }

    inline const std::vector<std::string>& config_subcommands() {
        static const std::vector<std::string> subs = {
            "list",
            "get",
            "set",
            "path",
            "reset",
        };
        return subs;
    }

    inline const std::vector<std::string>& env_subcommands() {
        static const std::vector<std::string> subs = {
            "list",
            "load",
            "save",
            "new",
            "delete",
        };
        return subs;
    }

    namespace detail {

        // Appends all candidates matching the given prefix to `out`.
        // Assumes `candidates` is a stable, non-overlapping set.
        // `prefix` may be empty, in which case all candidates are included.
        inline void match_prefix(const std::string&              prefix,
                                 const std::vector<std::string>& candidates,
                                 replxx::Replxx::completions_t&  out) {
            for (const auto& c : candidates)
                if (prefix.empty() || starts_with(c, prefix))
                    out.emplace_back(c);
        }

        // Handles completion for `:config` commands.
        // - For `get`/`set`, delegates to Settings::all_keys().
        // - Assumes `parts` is tokenized input; correctness depends on
        //   split() producing a faithful representation.
        inline replxx::Replxx::completions_t
        complete_config(const std::vector<std::string>& parts,
                        const std::string&              last_word) {
            replxx::Replxx::completions_t out;
            if (parts.size() <= 2) {
                match_prefix(last_word, config_subcommands(), out);
            } else if (parts[1] == "get" || parts[1] == "set") {
                const auto keys = Settings::all_keys();
                match_prefix(last_word, keys, out);
            }
            return out;
        }

        // Handles completion for `:env` commands.
        // - Interacts with both global config and context.
        // - For `save`, completion is context-sensitive: after the subcommand,
        //   completes environment names; after an env name, completes variable
        //   names.
        // - Assumes g_config.list_envs() and g_ctx.all_names() are cheap to
        // call.
        inline replxx::Replxx::completions_t
        complete_env(const std::vector<std::string>& parts,
                     const std::string&              last_word,
                     Config&                         g_config,
                     Context&                        g_ctx) {
            replxx::Replxx::completions_t out;
            if (parts.size() <= 2) {
                match_prefix(last_word, env_subcommands(), out);
            } else if (parts[1] == "load" || parts[1] == "delete") {
                match_prefix(last_word, g_config.list_envs(), out);
            } else if (parts[1] == "save") {
                if (parts.size() == 2) {
                    match_prefix(last_word, g_config.list_envs(), out);
                } else {
                    match_prefix(last_word, g_ctx.all_names(), out);
                }
            }
            return out;
        }

        // Used for `:set` and `:unset` commands.
        // - Only completes variable names known to the current context.
        // - Assumes g_ctx.all_names() returns a stable snapshot.
        inline replxx::Replxx::completions_t
        complete_var_names(const std::string& last_word, Context& g_ctx) {
            replxx::Replxx::completions_t out;
            match_prefix(last_word, g_ctx.all_names(), out);
            return out;
        }

        // Completion for `:simplify` flags.
        // - Flags are hardcoded; must be kept in sync with parser.
        // - Only invoked if last_word starts with '-'.
        inline replxx::Replxx::completions_t
        complete_simplify_flags(const std::string& last_word) {
            static const std::vector<std::string> flags = {
                "-vars", "-isolated", "-fraction"};
            replxx::Replxx::completions_t out;
            match_prefix(last_word, flags, out);
            return out;
        }

    } // namespace detail

    // Registers the completion callback with the given Replxx instance.
    // - The callback is stateful, capturing references to config and context.
    // - Completion logic is sensitive to whitespace and tokenization;
    //   leading whitespace is trimmed to avoid spurious context lengths.
    // - The callback must set `contextLen` to the length of the suffix being
    // completed.
    // - The logic here must remain consistent with the REPL's command parser;
    //   any changes to command syntax must be reflected here to avoid
    //   completion drift.
    // - Performance: All completion lists are static or derived from
    //   in-memory state; no I/O or heavy computation is performed.
    inline void
    setup_completions(replxx::Replxx& rx, Config& g_config, Context& g_ctx) {
        using std::string;
        using std::vector;

        rx.set_completion_callback([&g_config, &g_ctx](const string& input,
                                                       int&          contextLen)
                                       -> replxx::Replxx::completions_t {
            replxx::Replxx::completions_t completions;

            string                        trimmed = input;
            size_t                        s = trimmed.find_first_not_of(" \t");
            if (s == string::npos)
                trimmed.clear();
            else
                trimmed = trimmed.substr(s);

            // Single-word completion: only complete the first token.
            if (trimmed.find(' ') == string::npos) {
                contextLen = static_cast<int>(trimmed.size());
                detail::match_prefix(trimmed, all_commands(), completions);
                return completions;
            }

            // Multi-word completion: isolate the last word for context.
            size_t last_space    = trimmed.find_last_of(' ');
            string last_word     = (last_space != string::npos)
                                       ? trimmed.substr(last_space + 1)
                                       : trimmed;
            contextLen           = static_cast<int>(last_word.size());

            vector<string> parts = split(trimmed);

            if (starts_with(trimmed, ":config ")) {
                completions = detail::complete_config(parts, last_word);
            } else if (starts_with(trimmed, ":env ")) {
                completions =
                    detail::complete_env(parts, last_word, g_config, g_ctx);
            } else if (starts_with(trimmed, ":set ")) {
                if (parts.size() <= 2)
                    completions = detail::complete_var_names(last_word, g_ctx);
            } else if (starts_with(trimmed, ":unset ")) {
                completions = detail::complete_var_names(last_word, g_ctx);
            } else if (starts_with(trimmed, ":simplify ") &&
                       starts_with(last_word, "-")) {
                completions = detail::complete_simplify_flags(last_word);
            }

            return completions;
        });

        rx.set_word_break_characters(" \t");
    }

} // namespace math_solver
