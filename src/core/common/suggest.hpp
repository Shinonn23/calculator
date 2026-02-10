#ifndef SUGGEST_H
#define SUGGEST_H

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

namespace math_solver {

    // Compute Levenshtein edit distance between two strings
    inline int edit_distance(const std::string& a, const std::string& b) {
        size_t              m = a.size(), n = b.size();
        std::vector<size_t> prev(n + 1), curr(n + 1);

        for (size_t j = 0; j <= n; ++j)
            prev[j] = j;

        for (size_t i = 1; i <= m; ++i) {
            curr[0] = i;
            for (size_t j = 1; j <= n; ++j) {
                size_t cost = (a[i - 1] == b[j - 1]) ? 0 : 1;
                curr[j]     = std::min({
                    prev[j] + 1,       // deletion
                    curr[j - 1] + 1,   // insertion
                    prev[j - 1] + cost // substitution
                });
            }
            std::swap(prev, curr);
        }

        return static_cast<int>(prev[n]);
    }

    // Find the closest matching candidate within max_distance.
    // Returns the best match, or nullopt if none is close enough.
    inline std::optional<std::string>
    suggest(const std::string&              input,
            const std::vector<std::string>& candidates, int max_distance = 2) {
        std::optional<std::string> best;
        int                        best_dist = max_distance + 1;

        for (const auto& candidate : candidates) {
            int dist = edit_distance(input, candidate);
            if (dist < best_dist) {
                best_dist = dist;
                best      = candidate;
            }
        }

        return best;
    }

    // Find all candidates within max_distance, sorted by distance (ascending).
    inline std::vector<std::string>
    suggest_all(const std::string&              input,
                const std::vector<std::string>& candidates,
                int                             max_distance = 2) {
        // Pair: (distance, candidate)
        std::vector<std::pair<int, std::string>> matches;

        for (const auto& candidate : candidates) {
            int dist = edit_distance(input, candidate);
            if (dist <= max_distance && dist > 0) {
                matches.emplace_back(dist, candidate);
            }
        }

        std::sort(matches.begin(), matches.end());

        std::vector<std::string> result;
        result.reserve(matches.size());
        for (auto& [d, c] : matches) {
            result.push_back(std::move(c));
        }
        return result;
    }

} // namespace math_solver

#endif
