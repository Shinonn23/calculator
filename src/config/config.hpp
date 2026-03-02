#pragma once

#include "config/environment.hpp"
#include "config/settings.hpp"
#include "diagnostics/result.hpp"

#include <string>
#include <unordered_map>
#include <vector>

namespace math_solver {

    class Config {
        public:
        Settings&                  settings() { return settings_; }
        const Settings&            settings() const { return settings_; }
        const std::string&         file_path() const { return file_path_; }

        // Returns true if an environment with the given name exists in `envs_`.
        // Invariant: Environment names are unique within this Config.
        bool                       env_exists(const std::string& name) const;

        // Returns a mutable pointer to the requested environment.
        // Returns an error if the environment does not exist.
        Result<Environment*>       get_env(const std::string& name);

        // Returns an immutable pointer to the requested environment.
        // Returns an error if the environment does not exist.
        Result<const Environment*> get_env(const std::string& name) const;

        // Inserts a new environment with the given name.
        // Returns an error if the environment already exists.
        Result<bool>               create_env(const std::string& name);

        // Removes the environment with the given name.
        // Returns an error if the environment does not exist.
        Result<bool>               delete_env(const std::string& name);

        // Returns a list of all environment names in insertion order.
        // Note: unordered_map does not guarantee order; consumers must not rely
        // on ordering.
        std::vector<std::string>   list_envs() const;

        // Overwrites all variables in the specified environment with the
        // provided map. If the environment does not exist, this is a no-op.
        void                       save_env_variables(
                                  const std::string&                                  env_name,
                                  const std::unordered_map<std::string, std::string>& vars);

        // Renames the environment with the given name to a new name.
        // Returns an error if the environment does not exist.
        Result<bool> rename_env(const std::string& src,
                                const std::string& dest);

        // Copies the environment with the given name to a new environment.
        Result<bool> copy_env(const std::string& src, const std::string& dest);

        // Resets all settings to their default values.
        // Does not affect environments.
        void         reset_settings() {
            settings_ = Settings{};
            settings_.apply_to_globals();
        }

        // Resolves the canonical config file path for this process.
        // May consult environment variables or platform-specific conventions.
        static std::string resolve_config_path();

        // Loads configuration from the specified path, or from the default if
        // empty. May perform migration from legacy formats.
        void               load(const std::string& path = "");

        // Persists the current configuration to disk at `file_path_`.
        // Atomicity is not guaranteed; partial writes may corrupt the file.
        void               save() const;

        private:
        Settings                                     settings_;

        // Maps environment names to their corresponding Environment objects.
        // Invariant: No duplicate keys; all names are valid UTF-8.
        std::unordered_map<std::string, Environment> envs_;

        // Absolute path to the config file currently loaded or saved.
        // May be empty if the config has not been persisted.
        std::string                                  file_path_;

        // Initializes the default environment if none exist.
        // Called during initial load or after migration.
        void                                         create_default_env();

        // Populates settings and environments with hardcoded defaults.
        // Used on first run or after reset.
        void                                         create_defaults();

        // Migrates legacy settings from older config formats.
        // Must be idempotent and safe to call multiple times.
        void migrate_legacy_settings(const json& j);
    };

} // namespace math_solver