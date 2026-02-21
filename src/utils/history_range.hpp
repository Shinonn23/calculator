#pragma once

#include <algorithm>
#include <stdexcept>
#include <string>
#include <vector>

namespace math_solver {

    struct HistoryRange {
        // Parses a selector string into a sorted, deduplicated vector of
        // 1-based indices.
        //
        // Invariants:
        //   - All returned indices are >= 1.
        //   - Result is sorted and contains no duplicates.
        //
        // Correctness:
        //   - Throws std::invalid_argument on malformed input, inverted ranges,
        //   or index < 1.
        //   - Does not check for upper bounds; caller must validate against
        //   actual history size.
        //
        // Subtlety:
        //   - Accepts both single indices and inclusive ranges (e.g., "4-6").
        //   - Handles empty tokens gracefully (e.g., "1,,2" is tolerated as
        //   "1,2").
        //   - Relies on std::stoi for numeric conversion; will propagate
        //   exceptions for non-numeric tokens.
        //
        // Performance:
        //   - Sorting and deduplication are O(n log n), where n is the number
        //   of indices parsed.
        static std::vector<int> parse(const std::string& selector) {
            std::vector<int> result;

            std::string      token;
            std::string      s = selector;
            s += ','; // sentinel to simplify parsing logic

            for (char c : s) {
                if (c == ',') {
                    if (token.empty()) {
                        token.clear();
                        continue;
                    }

                    auto dash = token.find('-');
                    if (dash != std::string::npos) {
                        int lo = std::stoi(token.substr(0, dash));
                        int hi = std::stoi(token.substr(dash + 1));

                        if (lo < 1 || hi < 1)
                            throw std::invalid_argument(
                                "history index must be >= 1");
                        if (lo > hi)
                            throw std::invalid_argument(
                                "invalid range: " + token +
                                " (start must be <= end)");

                        for (int i = lo; i <= hi; ++i)
                            result.push_back(i);
                    } else {
                        int idx = std::stoi(token);
                        if (idx < 1)
                            throw std::invalid_argument(
                                "history index must be >= 1");
                        result.push_back(idx);
                    }

                    token.clear();
                } else {
                    token += c;
                }
            }

            // Ensure result is sorted and deduplicated before returning.
            std::sort(result.begin(), result.end());
            result.erase(std::unique(result.begin(), result.end()),
                         result.end());

            return result;
        }
    };

} // namespace math_solver
