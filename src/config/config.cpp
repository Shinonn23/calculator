#include "config/config.hpp"
#include "config/environment.hpp"
#include "config/settings.hpp"
#include "diagnostics/kinds/env_errors.hpp"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

namespace math_solver {

    using json = nlohmann::json;

    std::string Settings::set(const std::string& key, const std::string& val) {
        // Handles dynamic settings updates at runtime.
        // Returns error string on invalid input, else empty string.
        // Invariant: All settings must remain within documented bounds after
        // mutation.
        auto parse_bool = [](const std::string& v, bool& out) -> bool {
            if (v == "true" || v == "1" || v == "on") {
                out = true;
                return true;
            }
            if (v == "false" || v == "0" || v == "off") {
                out = false;
                return true;
            }
            return false;
        };

        auto parse_int = [](const std::string& v, int& out, int lo,
                            int hi) -> std::string {
            try {
                int n = std::stoi(v);
                if (n < lo || n > hi)
                    return "must be " + std::to_string(lo) + "-" +
                           std::to_string(hi);
                out = n;
                return "";
            } catch (...) {
                return "must be an integer";
            }
        };

        if (key == "output.mode") {
            if (val != "auto" && val != "decimal" && val != "exact")
                return "must be 'auto', 'decimal', or 'exact'";
            output_mode = val;
        } else if (key == "output.decimals") {
            return parse_int(val, output_decimals, 0, 15);
        } else if (key == "output.fraction") {
            if (!parse_bool(val, output_fraction))
                return "must be true/false";
        } else if (key == "output.trailing_zeros") {
            if (!parse_bool(val, output_trailing_zeros))
                return "must be true/false";
        } else if (key == "output.thousands_sep") {
            if (!parse_bool(val, output_thousands_sep))
                return "must be true/false";
        } else if (key == "solver.tolerance") {
            try {
                double v = std::stod(val);
                if (v <= 0)
                    return "must be > 0";
                solver_tolerance = v;
            } catch (...) {
                return "must be a positive number";
            }
        } else if (key == "solver.max_iter") {
            return parse_int(val, solver_max_iter, 1, 1000000);
        } else if (key == "history.size") {
            if (auto err = parse_int(val, history_size, 1, 100000);
                !err.empty())
                return err;
        } else if (key == "history.dedup") {
            if (!parse_bool(val, history_dedup))
                return "must be true/false";
        } else if (key == "history.ignore") {
            history_ignore = val;
        } else if (key == "repl.prompt") {
            repl_prompt = val;
        } else if (key == "repl.show_timing") {
            if (!parse_bool(val, repl_show_timing))
                return "must be true/false";
        } else if (key == "repl.auto_save_env") {
            if (!parse_bool(val, repl_auto_save_env))
                return "must be true/false";
        } else if (key == "repl.confirm_delete") {
            if (!parse_bool(val, repl_confirm_delete))
                return "must be true/false";
        } else if (key == "auto_load_env") {
            auto_load_env = val;
        } else {
            return "unknown setting '" + key + "'";
        }

        return "";
    }

    json Settings::to_json() const {
        // Serializes all settings fields.
        // Invariant: Output must be compatible with from_json and stable across
        // versions.
        return {
            {"output",
             {
                 {"mode", output_mode},
                 {"decimals", output_decimals},
                 {"fraction", output_fraction},
                 {"trailing_zeros", output_trailing_zeros},
                 {"thousands_sep", output_thousands_sep},
             }                             },
            {"solver",
             {
                 {"tolerance", solver_tolerance},
                 {"max_iter", solver_max_iter},
             }                             },
            {"history",
             {
                 {"size", history_size},
                 {"dedup", history_dedup},
                 {"ignore", history_ignore},
             }                             },
            {"repl",
             {
                 {"prompt", repl_prompt},
                 {"show_timing", repl_show_timing},
                 {"auto_save_env", repl_auto_save_env},
                 {"confirm_delete", repl_confirm_delete},
             }                             },
            {"auto_load_env", auto_load_env},
        };
    }

    Settings Settings::from_json(const json& j) {
        // Loads settings from nested JSON.
        // Defensive: missing keys fallback to hardcoded defaults.
        Settings s;

        auto     get_bool = [&](const json& node, const char* key, bool def) {
            return node.contains(key) ? node[key].get<bool>() : def;
        };
        auto get_int = [&](const json& node, const char* key, int def) {
            return node.contains(key) ? node[key].get<int>() : def;
        };
        auto get_str = [&](const json& node, const char* key,
                           const std::string& def) {
            return node.contains(key) ? node[key].get<std::string>() : def;
        };
        auto get_dbl = [&](const json& node, const char* key, double def) {
            return node.contains(key) ? node[key].get<double>() : def;
        };

        if (j.contains("output") && j["output"].is_object()) {
            const auto& o           = j["output"];
            s.output_mode           = get_str(o, "mode", "auto");
            s.output_decimals       = get_int(o, "decimals", 6);
            s.output_fraction       = get_bool(o, "fraction", false);
            s.output_trailing_zeros = get_bool(o, "trailing_zeros", false);
            s.output_thousands_sep  = get_bool(o, "thousands_sep", false);
        }

        if (j.contains("solver") && j["solver"].is_object()) {
            const auto& sv     = j["solver"];
            s.solver_tolerance = get_dbl(sv, "tolerance", 1e-12);
            s.solver_max_iter  = get_int(sv, "max_iter", 1000);
        }

        if (j.contains("history") && j["history"].is_object()) {
            const auto& h    = j["history"];
            s.history_size   = get_int(h, "size", 1000);
            s.history_dedup  = get_bool(h, "dedup", true);
            s.history_ignore = get_str(h, "ignore", "");
        }

        if (j.contains("repl") && j["repl"].is_object()) {
            const auto& r         = j["repl"];
            s.repl_prompt         = get_str(r, "prompt", "> ");
            s.repl_show_timing    = get_bool(r, "show_timing", false);
            s.repl_auto_save_env  = get_bool(r, "auto_save_env", true);
            s.repl_confirm_delete = get_bool(r, "confirm_delete", true);
        }

        if (j.contains("auto_load_env"))
            s.auto_load_env = j["auto_load_env"].get<std::string>();

        return s;
    }

    Settings Settings::migrate_legacy(const json& j) {
        // Legacy migration: maps flat config keys to new nested structure.
        // Only migrates known legacy keys; all others use defaults.
        Settings s;

        if (j.contains("precision") && j["precision"].is_number())
            s.output_decimals = j["precision"].get<int>();

        if (j.contains("fraction_mode") && j["fraction_mode"].is_boolean())
            s.output_fraction = j["fraction_mode"].get<bool>();

        if (j.contains("history_size") && j["history_size"].is_number())
            s.history_size = j["history_size"].get<int>();

        if (j.contains("auto_load_env"))
            s.auto_load_env = j["auto_load_env"].get<std::string>();

        return s;
    }

    json Environment::to_json() const {
        // Serializes all variables as string values.
        // Invariant: All variable values must be stringified for round-trip
        // compatibility.
        json vars = json::object();
        for (const auto& [k, v] : variables)
            vars[k] = v;
        return {
            {"variables", vars}
        };
    }

    Environment Environment::from_json(const std::string& env_name,
                                       const json&        j) {
        // Loads environment variables from JSON.
        // Handles legacy numeric values by converting to string and normalizing
        // trailing zeros.
        Environment env;
        env.name = env_name;

        if (!j.contains("variables") || !j["variables"].is_object())
            return env;

        for (auto& [k, v] : j["variables"].items()) {
            if (v.is_string()) {
                env.variables[k] = v.get<std::string>();
            } else if (v.is_number()) {
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

    bool Config::env_exists(const std::string& name) const {
        // O(1) lookup. Used to enforce uniqueness and existence invariants.
        return envs_.count(name) > 0;
    }

    Result<Environment*> Config::get_env(const std::string& name) {
        auto it = envs_.find(name);
        if (it == envs_.end())
            return Result<Environment*>::err(errors::env_not_found(
                "<config>", name, *this, __FILE__, __LINE__));
        return Result<Environment*>::ok(&it->second);
    }

    Result<const Environment*> Config::get_env(const std::string& name) const {
        auto it = envs_.find(name);
        if (it == envs_.end())
            return Result<const Environment*>::err(errors::env_not_found(
                "<config>", name, *this, __FILE__, __LINE__));
        return Result<const Environment*>::ok(&it->second);
    }

    Result<bool> Config::create_env(const std::string& name) {
        if (envs_.count(name))
            return Result<bool>::err(
                Diagnostic::make("environment '" + name + "' already exists",
                                 "E0602")
                    .with_location(__FILE__, __LINE__));
        envs_[name].name = name;
        return Result<bool>::ok(true);
    }

    Result<bool> Config::delete_env(const std::string& name) {
        if (!envs_.erase(name))
            return Result<bool>::err(errors::env_not_found(
                "<config>", name, *this, __FILE__, __LINE__));
        return Result<bool>::ok(true);
    }

    std::vector<std::string> Config::list_envs() const {
        // Returns sorted list of environment names for deterministic iteration.
        std::vector<std::string> names;
        names.reserve(envs_.size());
        for (const auto& [name, _] : envs_)
            names.push_back(name);
        std::sort(names.begin(), names.end());
        return names;
    }

    void Config::save_env_variables(
        const std::string&                                  env_name,
        const std::unordered_map<std::string, std::string>& vars) {
        // Overwrites all variables for the given environment.
        // Used by REPL and scripting subsystems.
        envs_[env_name].name      = env_name;
        envs_[env_name].variables = vars;
    }

    Result<bool> Config::rename_env(const std::string& src,
                                    const std::string& dest) {
        if (!env_exists(src))
            return Result<bool>::err(errors::env_not_found(
                "<config>", src, *this, __FILE__, __LINE__));
        if (env_exists(dest))
            return Result<bool>::err(
                Diagnostic::make("environment '" + dest + "' already exists",
                                 "E0602")
                    .with_location(__FILE__, __LINE__));
        envs_[dest] = envs_[src];
        envs_.erase(src);
        return Result<bool>::ok(true);
    }

    Result<bool> Config::copy_env(const std::string& src,
                                  const std::string& dest) {
        if (!env_exists(src))
            return Result<bool>::err(errors::env_not_found(
                "<config>", src, *this, __FILE__, __LINE__));
        if (env_exists(dest))
            return Result<bool>::err(
                Diagnostic::make("environment '" + dest + "' already exists",
                                 "E0602")
                    .with_location(__FILE__, __LINE__));
        envs_[dest] = envs_[src];
        return Result<bool>::ok(true);
    }

    std::string Config::resolve_config_path() {
        // Determines config file location.
        // - Prefers local directory if file exists.
        // - Otherwise, uses platform-specific config directory.
        // - Ensures parent directories exist before returning.
        namespace fs   = std::filesystem;
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
        if (!dir.empty()) {
            std::filesystem::create_directories(dir);
            return (dir / "math_solver.json").string();
        }

        return local.string();
    }

    void Config::load(const std::string& path) {
        // Loads config from disk, handling both legacy and current formats.
        // On parse failure, falls back to defaults and emits warning.
        // Invariant: After load, at least one environment ("default") exists.
        namespace fs = std::filesystem;
        file_path_   = path.empty() ? resolve_config_path() : path;

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

            if (j.contains("settings")) {
                const auto& s         = j["settings"];

                // Detect legacy format by presence of flat keys.
                bool        is_legacy = s.contains("precision") ||
                                 s.contains("fraction_mode") ||
                                 s.contains("history_size");

                settings_ = is_legacy ? Settings::migrate_legacy(s)
                                      : Settings::from_json(s);
            }

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

    void Config::save() const {
        // Serializes config to disk.
        // Ensures parent directories exist before writing.
        // Output is stable and human-readable (indent=2).
        namespace fs = std::filesystem;
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

    void Config::create_default_env() {
        // Installs the canonical "default" environment.
        // Invariant: "default" always contains standard mathematical constants.
        Environment env;
        env.name      = "default";
        env.variables = {
            {"pi",  "3.14159265358979"},
            {"e",   "2.71828182845905"},
            {"tau", "6.28318530717959"},
        };
        envs_["default"] = std::move(env);
    }

    void Config::create_defaults() {
        // Resets all config state to hardcoded defaults.
        settings_ = Settings{};
        envs_.clear();
        create_default_env();
    }

} // namespace math_solver