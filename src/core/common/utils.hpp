#ifndef UTILS_H
#define UTILS_H
#include <cmath>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

#include "config.hpp"

using namespace std;

namespace math_solver {

    // Format a double value with controlled precision, stripping trailing
    // zeros. precision: max number of decimal places (default 6)
    inline std::string format_double(double value, int precision = 6) {
        if (std::isnan(value))
            return "NaN";
        if (std::isinf(value))
            return value > 0 ? "Infinity" : "-Infinity";

        // Use fixed notation with requested precision
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(precision) << value;
        std::string s   = oss.str();

        // Strip trailing zeros after decimal point
        size_t      dot = s.find('.');
        if (dot != std::string::npos) {
            size_t last_nonzero = s.find_last_not_of('0');
            if (last_nonzero != std::string::npos && last_nonzero > dot) {
                s.erase(last_nonzero + 1);
            } else if (last_nonzero == dot) {
                // Remove the dot too — it's an integer
                s.erase(dot);
            }
        }

        // Avoid "-0"
        if (s == "-0")
            s = "0";

        return s;
    }

    inline string trim(const string& s) {
        size_t start = s.find_first_not_of(" \t\r\n");
        if (start == string::npos)
            return "";
        size_t end = s.find_last_not_of(" \t\r\n");
        return s.substr(start, end - start + 1);
    }

    inline vector<string> split(const string& s) {
        vector<string> tokens;
        istringstream  iss(s);
        string         token;
        while (iss >> token)
            tokens.push_back(token);
        return tokens;
    }

    inline bool starts_with(const string& s, const string& prefix) {
        return s.size() >= prefix.size() &&
               s.compare(0, prefix.size(), prefix) == 0;
    }

    inline string fmt(double value, const Config& g_config) {
        return format_double(value, g_config.settings().precision);
    }

    inline string get_history_file_path() {
        namespace fs = std::filesystem;
#ifdef _WIN32
        char*  appdata = nullptr;
        size_t len     = 0;
        _dupenv_s(&appdata, &len, "APPDATA");
        if (appdata) {
            fs::path dir = fs::path(appdata) / "math-solver";
            free(appdata); // must free the buffer allocated by _dupenv_s
            if (!fs::exists(dir))
                fs::create_directories(dir);
            return (dir / "history.txt").string();
        }
#else
        char*  home = nullptr;
        size_t len  = 0;
        _dupenv_s(&home, &len, "HOME");
        if (home) {
            fs::path dir = fs::path(home) / ".config" / "math-solver";
            free(home); // must free the buffer allocated by _dupenv_s
            if (!fs::exists(dir))
                fs::create_directories(dir);
            return (dir / "history.txt").string();
        }
#endif
        return ".math_solver_history";
    }

} // namespace math_solver

#endif
