#pragma once

#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>

namespace math_solver {

    namespace fs = std::filesystem;
    using json   = nlohmann::json;

    struct Settings {
        int         precision     = 6;
        bool        fraction_mode = false;
        int         history_size  = 1000;
        std::string auto_load_env = "default";

        // Serialization for persistence. All fields must be kept in sync with
        // deserialization logic in from_json. Any additions require versioning
        // consideration for backward compatibility.
        json        to_json() const {
            return {
                {"precision",     precision    },
                {"fraction_mode", fraction_mode},
                {"history_size",  history_size },
                {"auto_load_env", auto_load_env},
            };
        }

        // Deserialization. Defensive: missing fields are left at defaults.
        // Invariant: All fields must be valid after construction.
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

        static std::vector<std::string> all_keys() {
            return {
                "precision", "fraction_mode", "history_size", "auto_load_env"};
        }

        // Used for validating user input and config file keys.
        static bool is_valid_key(const std::string& key) {
            for (const auto& k : all_keys())
                if (k == key)
                    return true;
            return false;
        }

        // Returns string representation of the setting value.
        // Used for UI and scripting. All values must be representable as
        // string.
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

        // Returns empty string on success, error message on failure.
        // All validation is performed here; invariants must be preserved.
        // Used by scripting and UI. Error messages are user-facing.
        std::string set(const std::string& key, const std::string& value) {
            if (key == "precision") {
                try {
                    int v = std::stoi(value);
                    // Precision is bounded for compatibility with
                    // floating-point formatting.
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
                    // history_size must be positive; zero disables history.
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
            return "";
        }
    };

    struct Environment {
        std::string                                  name;
        std::unordered_map<std::string, std::string> variables;

        // Serializes all variables as strings. No type information is
        // preserved. Invariant: All variable values must be
        // string-representable.
        json                                         to_json() const {
            json vars = json::object();
            for (const auto& [k, v] : variables)
                vars[k] = v;
            return {
                {"variables", vars}
            };
        }

        // Deserialization. Accepts both string and numeric values for backward
        // compatibility. Numeric values are converted to strings with minimal
        // trailing zeros. This logic must be kept in sync with to_json.
        static Environment from_json(const std::string& env_name,
                                     const json&        j) {
            Environment env;
            env.name = env_name;
            if (!j.contains("variables") || !j["variables"].is_object())
                return env;

            for (auto& [k, v] : j["variables"].items()) {
                if (v.is_string()) {
                    env.variables[k] = v.get<std::string>();
                } else if (v.is_number()) {
                    // Legacy: numeric → string, strip trailing zeros for
                    // round-trip stability.
                    double      num = v.get<double>();
                    std::string s   = std::to_string(num);
                    if (auto dot = s.find('.'); dot != std::string::npos) {
                        s.erase(s.find_last_not_of('0') + 1);
                        if (s.back() == '.')
                            s.pop_back();
                    }
                    env.variables[k] = s;
                }
            }
            return env;
        }
    };

    class Config {
        public:
        Config() = default;

        Settings&          settings() { return settings_; }
        const Settings&    settings() const { return settings_; }
        const std::string& file_path() const { return file_path_; }

        // Returns true if the named environment exists.
        // Used for validation and UI.
        bool               env_exists(const std::string& name) const {
            return envs_.count(name) > 0;
        }

        // Returns mutable reference to environment. Throws if not found.
        // Used by scripting and UI. Invariant: name must exist.
        Environment& get_env(const std::string& name) {
            auto it = envs_.find(name);
            if (it == envs_.end())
                throw std::runtime_error("environment '" + name +
                                         "' not found");
            return it->second;
        }

        // Returns const reference to environment. Throws if not found.
        const Environment& get_env(const std::string& name) const {
            auto it = envs_.find(name);
            if (it == envs_.end())
                throw std::runtime_error("environment '" + name +
                                         "' not found");
            return it->second;
        }

        // Creates a new environment with the given name.
        // Panics if already exists. Used by UI and scripting.
        void create_env(const std::string& name) {
            if (envs_.count(name))
                throw std::runtime_error("environment '" + name +
                                         "' already exists");
            envs_[name].name = name;
        }

        // Removes the named environment. Panics if not found.
        void delete_env(const std::string& name) {
            if (!envs_.erase(name))
                throw std::runtime_error("environment '" + name +
                                         "' not found");
        }

        // Returns sorted list of environment names. Used for UI.
        std::vector<std::string> list_envs() const {
            std::vector<std::string> names;
            names.reserve(envs_.size());
            for (const auto& [name, _] : envs_)
                names.push_back(name);
            std::sort(names.begin(), names.end());
            return names;
        }

        // Overwrites all variables in the named environment.
        // Used for scripting and UI. If env does not exist, it is created.
        void save_env_variables(
            const std::string&                                  env_name,
            const std::unordered_map<std::string, std::string>& vars) {
            envs_[env_name].name      = env_name;
            envs_[env_name].variables = vars;
        }

        // Resets settings to defaults. Does not affect environments.
        void               reset_settings() { settings_ = Settings {}; }

        // Returns the resolved config file path.
        // On Unix, prefers $HOME/.config/math-solver/math_solver.json.
        // On Windows, prefers %APPDATA%\math-solver\math_solver.json.
        // Falls back to local directory if env vars are missing.
        static std::string resolve_config_path() {
            fs::path local = fs::current_path() / "math_solver.json";
            if (fs::exists(local))
                return local.string();

            fs::path dir;
#ifdef _WIN32
            if (const char* p = std::getenv("APPDATA"))
                dir = fs::path(p) / "math-solver";
#else
            if (const char* p = std::getenv("HOME"))
                dir = fs::path(p) / ".config" / "math-solver";
#endif
            if (!dir.empty())
                return (dir / "math_solver.json").string();
            return local.string();
        }

        // Loads config from disk. If file is missing or unreadable, creates
        // defaults and persists them. If parsing fails, prints warning and
        // falls back to defaults. Invariant: after load, settings and at least
        // the "default" environment exist.
        void load(const std::string& path = "") {
            file_path_ = path.empty() ? resolve_config_path() : path;

            if (!fs::exists(file_path_)) {
                create_defaults();
                save();
                return;
            }

            std::ifstream ifs(file_path_);
            if (!ifs) {
                create_defaults();
                return;
            }

            try {
                json j = json::parse(ifs);
                if (j.contains("settings"))
                    settings_ = Settings::from_json(j["settings"]);
                if (j.contains("environments")) {
                    envs_.clear();
                    for (auto& [name, val] : j["environments"].items())
                        envs_[name] = Environment::from_json(name, val);
                }
                if (!env_exists("default"))
                    create_default_env();
            } catch (const json::parse_error& e) {
                std::cerr << "Warning: failed to parse config (" << e.what()
                          << "). Using defaults.\n";
                create_defaults();
            }
        }

        // Serializes config to disk. Creates parent directories if needed.
        // If file_path_ is empty, does nothing. Used for persistence after
        // mutation. Atomicity is not guaranteed.
        void save() const {
            if (file_path_.empty())
                return;
            if (auto dir = fs::path(file_path_).parent_path(); !dir.empty())
                fs::create_directories(dir);

            json j;
            j["settings"]     = settings_.to_json();
            j["environments"] = json::object();
            for (const auto& [name, env] : envs_)
                j["environments"][name] = env.to_json();

            if (std::ofstream ofs(file_path_); ofs)
                ofs << j.dump(2) << '\n';
        }

        private:
        Settings                                     settings_;
        std::unordered_map<std::string, Environment> envs_;
        std::string                                  file_path_;

        // Ensures the "default" environment exists with standard constants.
        // Called after load if missing, and during initial creation.
        void                                         create_default_env() {
            Environment env;
            env.name      = "default";
            env.variables = {
                {"pi",  "3.14159265358979"},
                {"e",   "2.71828182845905"},
                {"tau", "6.28318530717959"}
            };
            envs_["default"] = std::move(env);
        }

        // Resets all state to defaults. Used on parse failure or first run.
        // Invariant: after call, settings_ is default and envs_ contains only
        // the "default" environment.
        void create_defaults() {
            settings_ = Settings {};
            envs_.clear();
            create_default_env();
        }
    };

} // namespace math_solver