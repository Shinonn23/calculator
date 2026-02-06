#ifndef UTILS_H
#define UTILS_H

#include <sstream>
#include <string>
#include <vector>

using namespace std;

namespace math_solver {
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
        while (iss >> token) {
            tokens.push_back(token);
        }
        return tokens;
    }

    inline bool starts_with(const string& s, const string& prefix) {
        return s.size() >= prefix.size() &&
               s.compare(0, prefix.size(), prefix) == 0;
    }

    inline bool ends_with(const string& s, const string& suffix) {
        return s.size() >= suffix.size() &&
               s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
    }

} // namespace math_solver
#endif