#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace math_solver {

    using json = nlohmann::json;

    struct Settings {
        // Output configuration. Invariant: output_mode must be one of {"auto",
        // "decimal", "exact"}. Changing output_decimals or output_fraction may
        // impact downstream formatting logic.
        std::string                     output_mode           = "auto";
        int                             output_decimals       = 6;
        bool                            output_fraction       = false;
        bool                            output_trailing_zeros = false;
        bool                            output_thousands_sep  = false;

        // Solver parameters. These values are assumed to be respected by all
        // solver routines. solver_tolerance must be positive and sufficiently
        // small for numerical stability.
        double                          solver_tolerance      = 1e-12;
        int                             solver_max_iter       = 1000;

        // History management. history_size is a hard cap; exceeding this
        // triggers eviction. history_dedup enables deduplication logic in the
        // history subsystem.
        int                             history_size          = 1000;
        bool                            history_dedup         = true;
        std::string                     history_ignore        = "";

        // REPL configuration. repl_auto_save_env and repl_confirm_delete
        // interact with session persistence.
        std::string                     repl_prompt           = "> ";
        bool                            repl_show_timing      = false;
        bool                            repl_auto_save_env    = true;
        bool                            repl_confirm_delete   = true;

        // Miscellaneous settings. auto_load_env is used for environment
        // bootstrapping.
        std::string                     auto_load_env         = "";

        // Returns the canonical list of all supported setting keys.
        // Invariant: all_keys() must be kept in sync with get/set logic.
        static std::vector<std::string> all_keys() {
            return {
                "output.mode",          "output.decimals",
                "output.fraction",      "output.trailing_zeros",
                "output.thousands_sep", "solver.tolerance",
                "solver.max_iter",      "history.size",
                "history.dedup",        "history.ignore",
                "repl.prompt",          "repl.show_timing",
                "repl.auto_save_env",   "repl.confirm_delete",
                "auto_load_env",
            };
        }

        // Returns true if the key is recognized by this version of Settings.
        // Used for input validation and migration logic.
        static bool is_valid_key(const std::string& key) {
            for (const auto& k : all_keys())
                if (k == key)
                    return true;
            return false;
        }

        // Returns the string representation of the setting for the given key.
        // Returns "" for unknown keys. Must be kept in sync with all_keys().
        std::string get(const std::string& key) const {
            if (key == "output.mode")
                return output_mode;
            if (key == "output.decimals")
                return std::to_string(output_decimals);
            if (key == "output.fraction")
                return output_fraction ? "true" : "false";
            if (key == "output.trailing_zeros")
                return output_trailing_zeros ? "true" : "false";
            if (key == "output.thousands_sep")
                return output_thousands_sep ? "true" : "false";
            if (key == "solver.tolerance") {
                int exponent =
                    static_cast<int>(std::round(-std::log10(solver_tolerance)));
                return "1e-" + std::to_string(exponent);
            }
            if (key == "solver.max_iter")
                return std::to_string(solver_max_iter);
            if (key == "history.size")
                return std::to_string(history_size);
            if (key == "history.dedup")
                return history_dedup ? "true" : "false";
            if (key == "history.ignore")
                return history_ignore;
            if (key == "repl.prompt")
                return repl_prompt;
            if (key == "repl.show_timing")
                return repl_show_timing ? "true" : "false";
            if (key == "repl.auto_save_env")
                return repl_auto_save_env ? "true" : "false";
            if (key == "repl.confirm_delete")
                return repl_confirm_delete ? "true" : "false";
            if (key == "auto_load_env")
                return auto_load_env;
            return "";
        }

        // Attempts to set the value for the given key.
        // Returns "" on success, or an error message on failure.
        // Invariant: set() must not leave the struct in an inconsistent state.
        std::string     set(const std::string& key, const std::string& value);

        // Serializes the current settings to JSON.
        // Must be kept in sync with from_json().
        json            to_json() const;

        // Constructs Settings from a JSON object.
        // Assumes input is well-formed; migration is handled separately.
        static Settings from_json(const json& j);

        // Migrates legacy configuration formats to the current schema.
        // Used during config loading to handle breaking changes or key renames.
        static Settings migrate_legacy(const json& j);
    };

} // namespace math_solver