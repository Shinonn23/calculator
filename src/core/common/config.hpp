#ifndef CONFIG_H
#define CONFIG_H

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace math_solver {

    namespace fs = std::filesystem;
    using json   = nlohmann::json;

    // ========================================================================
    // Settings — typed application settings
    // ========================================================================

    struct Settings {
        int         precision     = 6;         // Decimal precision for output
        bool        fraction_mode = false;     // Default --fraction flag
        int         history_size  = 1000;      // Max REPL history entries
        std::string auto_load_env = "default"; // Environment to load on start

        // Serialize to JSON
        json        to_json() const {
            return json{
                       {"precision",     precision    },
                       {"fraction_mode", fraction_mode},
                       {"history_size",  history_size },
                       {"auto_load_env", auto_load_env},
            };
        }

        // Deserialize from JSON (with defaults for missing keys)
        static Settings from_json(const json& j) {
            Settings s;
            if (j.contains("precision"))
                s.precision = j["precision"].get<int>();
            if (j.contains("fraction_mode"))
                s.fraction_mode = j["fraction_mode"].get<bool>();
            if (j.contains("history_size"))
                s.history_size = j["history_size"].get<int>();
            if (j.contains("auto_load_env"))
                s.auto_load_env = j["auto_load_env"].get<std::string>();
            return s;
        }

        // Get all setting names
        static std::vector<std::string> all_keys() {
            return {"precision", "fraction_mode", "history_size",
                    "auto_load_env"};
        }

        // Get a setting value as string by key
        std::string get(const std::string& key) const {
            if (key == "precision")
                return std::to_string(precision);
            if (key == "fraction_mode")
                return fraction_mode ? "true" : "false";
            if (key == "history_size")
                return std::to_string(history_size);
            if (key == "auto_load_env")
                return auto_load_env;
            return "";
        }

        // Set a setting value from string. Returns error message or empty on
        // success.
        std::string set(const std::string& key, const std::string& value) {
            if (key == "precision") {
                try {
                    int v = std::stoi(value);
                    if (v < 0 || v > 15)
                        return "precision must be 0-15";
                    precision = v;
                } catch (...) {
                    return "precision must be an integer (0-15)";
                }
            } else if (key == "fraction_mode") {
                if (value == "true" || value == "1" || value == "on")
                    fraction_mode = true;
                else if (value == "false" || value == "0" || value == "off")
                    fraction_mode = false;
                else
                    return "fraction_mode must be true/false";
            } else if (key == "history_size") {
                try {
                    int v = std::stoi(value);
                    if (v < 1)
                        return "history_size must be > 0";
                    history_size = v;
                } catch (...) {
                    return "history_size must be a positive integer";
                }
            } else if (key == "auto_load_env") {
                auto_load_env = value;
            } else {
                return "unknown setting '" + key + "'";
            }
            return ""; // success
        }

        // Check if a key is a valid setting name
        static bool is_valid_key(const std::string& key) {
            auto keys = all_keys();
            return std::find(keys.begin(), keys.end(), key) != keys.end();
        }
    };

    // ========================================================================
    // Environment — a named set of preset variables
    // ========================================================================

    struct Environment {
        std::string                             name;
        std::unordered_map<std::string, double> variables;

        json                                    to_json() const {
            json vars = json::object();
            for (const auto& [k, v] : variables) {
                vars[k] = v;
            }
            return json{
                                                   {"variables", vars}
            };
        }

        static Environment from_json(const std::string& name, const json& j) {
            Environment env;
            env.name = name;
            if (j.contains("variables") && j["variables"].is_object()) {
                for (auto& [k, v] : j["variables"].items()) {
                    if (v.is_number()) {
                        env.variables[k] = v.get<double>();
                    }
                }
            }
            return env;
        }
    };

    // ========================================================================
    // Config — manages settings and environments, persisted to JSON file
    // ========================================================================

    class Config {
        private:
        Settings                                     settings_;
        std::unordered_map<std::string, Environment> envs_;
        std::string                                  file_path_;

        public:
        Config() = default;

        // ---- Settings accessors -------------------------------------------

        Settings&       settings() { return settings_; }
        const Settings& settings() const { return settings_; }

        // ---- Environment accessors ----------------------------------------

        bool            env_exists(const std::string& name) const {
            return envs_.find(name) != envs_.end();
        }

        Environment& get_env(const std::string& name) {
            auto it = envs_.find(name);
            if (it == envs_.end())
                throw std::runtime_error("environment '" + name +
                                         "' not found");
            return it->second;
        }

        const Environment& get_env(const std::string& name) const {
            auto it = envs_.find(name);
            if (it == envs_.end())
                throw std::runtime_error("environment '" + name +
                                         "' not found");
            return it->second;
        }

        void create_env(const std::string& name) {
            if (envs_.find(name) != envs_.end())
                throw std::runtime_error("environment '" + name +
                                         "' already exists");
            Environment env;
            env.name    = name;
            envs_[name] = std::move(env);
        }

        void delete_env(const std::string& name) {
            auto it = envs_.find(name);
            if (it == envs_.end())
                throw std::runtime_error("environment '" + name +
                                         "' not found");
            envs_.erase(it);
        }

        std::vector<std::string> list_envs() const {
            std::vector<std::string> names;
            names.reserve(envs_.size());
            for (const auto& [name, _] : envs_) {
                names.push_back(name);
            }
            std::sort(names.begin(), names.end());
            return names;
        }

        // Save the current variables from Context into an environment
        void save_env_variables(
            const std::string&                             env_name,
            const std::unordered_map<std::string, double>& variables) {
            auto it = envs_.find(env_name);
            if (it == envs_.end()) {
                Environment env;
                env.name        = env_name;
                env.variables   = variables;
                envs_[env_name] = std::move(env);
            } else {
                it->second.variables = variables;
            }
        }

        // ---- Persistence --------------------------------------------------

        const std::string& file_path() const { return file_path_; }

        // Determine the config file path
        static std::string resolve_config_path() {
            // 1) Check current directory
            fs::path local_path = fs::current_path() / "math_solver.json";
            if (fs::exists(local_path))
                return local_path.string();

            // 2) Platform-specific config directory
            fs::path config_dir;
#ifdef _WIN32
            const char* appdata = std::getenv("APPDATA");
            if (appdata) {
                config_dir = fs::path(appdata) / "math-solver";
            }
#else
            const char* home = std::getenv("HOME");
            if (home) {
                config_dir = fs::path(home) / ".config" / "math-solver";
            }
#endif
            if (!config_dir.empty()) {
                fs::path cfg_path = config_dir / "math_solver.json";
                if (fs::exists(cfg_path))
                    return cfg_path.string();
                // Will create here if no local file
                return cfg_path.string();
            }

            // 3) Fallback to current directory
            return local_path.string();
        }

        // Load config from file. Creates default file if it doesn't exist.
        void load(const std::string& path = "") {
            file_path_ = path.empty() ? resolve_config_path() : path;

            if (!fs::exists(file_path_)) {
                // Create default config
                create_defaults();
                save();
                return;
            }

            std::ifstream ifs(file_path_);
            if (!ifs.is_open()) {
                create_defaults();
                return;
            }

            try {
                json j = json::parse(ifs);

                // Load settings
                if (j.contains("settings") && j["settings"].is_object()) {
                    settings_ = Settings::from_json(j["settings"]);
                }

                // Load environments
                envs_.clear();
                if (j.contains("environments") &&
                    j["environments"].is_object()) {
                    for (auto& [name, val] : j["environments"].items()) {
                        envs_[name] = Environment::from_json(name, val);
                    }
                }

                // Ensure "default" env exists
                if (!env_exists("default")) {
                    create_default_env();
                }

            } catch (const json::parse_error& e) {
                std::cerr << "Warning: failed to parse config file ("
                          << e.what() << "). Using defaults.\n";
                create_defaults();
            }
        }

        // Save current config to file
        void save() const {
            if (file_path_.empty())
                return;

            // Ensure directory exists
            fs::path dir = fs::path(file_path_).parent_path();
            if (!dir.empty() && !fs::exists(dir)) {
                fs::create_directories(dir);
            }

            json j;
            j["settings"]  = settings_.to_json();

            json envs_json = json::object();
            for (const auto& [name, env] : envs_) {
                envs_json[name] = env.to_json();
            }
            j["environments"] = envs_json;

            std::ofstream ofs(file_path_);
            if (ofs.is_open()) {
                ofs << j.dump(2) << std::endl;
            }
        }

        // Reset settings to defaults (keeps environments)
        void reset_settings() { settings_ = Settings(); }

        private:
        void create_defaults() {
            settings_ = Settings();
            envs_.clear();
            create_default_env();
        }

        void create_default_env() {
            Environment env;
            env.name             = "default";
            env.variables["pi"]  = 3.14159265358979;
            env.variables["e"]   = 2.71828182845905;
            env.variables["tau"] = 6.28318530717959;
            envs_["default"]     = std::move(env);
        }
    };

} // namespace math_solver

#endif
