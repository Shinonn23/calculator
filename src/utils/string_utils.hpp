#ifndef STRING_UTILS_H
#define STRING_UTILS_H

#include <sstream>
#include <string>
#include <vector>

namespace math_solver {

    // Trims ASCII whitespace from both ends of the input.
    // Assumes input is not null. Returns empty string if input is all
    // whitespace. Used to sanitize user input and prevent spurious parsing
    // errors.
    inline std::string trim(const std::string& s) {
        size_t start = s.find_first_not_of(" \t\r\n");
        if (start == std::string::npos)
            return "";
        size_t end = s.find_last_not_of(" \t\r\n");
        return s.substr(start, end - start + 1);
    }

    // Tokenizes the input string using whitespace as delimiter.
    // Relies on std::istringstream, which collapses consecutive whitespace.
    // Used in parser frontends; not suitable for preserving empty tokens.
    inline std::vector<std::string> split(const std::string& s) {
        std::vector<std::string> tokens;
        std::istringstream       iss(s);
        std::string              token;
        while (iss >> token)
            tokens.push_back(token);
        return tokens;
    }

    // Returns true if `s` begins with `prefix`.
    // Assumes both strings are valid UTF-8; does not check for codepoint
    // boundaries. Used in lexer and preprocessor for directive detection.
    inline bool starts_with(const std::string& s, const std::string& prefix) {
        return s.size() >= prefix.size() &&
               s.compare(0, prefix.size(), prefix) == 0;
    }

    // Returns true if `s` ends with `suffix`.
    // Assumes both strings are valid UTF-8; does not check for codepoint
    // boundaries. Used for file extension checks and similar suffix-based
    // logic.
    inline bool ends_with(const std::string& s, const std::string& suffix) {
        return s.size() >= suffix.size() &&
               s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
    }

    // Converts all ASCII characters in the input to lowercase.
    // Non-ASCII bytes are left unchanged; not locale-aware.
    // Used for case-insensitive comparisons in parser and symbol resolution.
    inline std::string to_lower(const std::string& s) {
        std::string result = s;
        for (char& c : result)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return result;
    }

    // Removes everything after the first '#' character.
    // Used to strip comments from input lines before parsing.
    // Assumes '#' is not valid in non-comment contexts.
    inline std::string strip_comment(const std::string& s) {
        size_t pos = s.find('#');
        if (pos != std::string::npos)
            return s.substr(0, pos);
        return s;
    }

} // namespace math_solver

#endif // STRING_UTILS_H
