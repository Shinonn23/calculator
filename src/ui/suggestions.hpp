#pragma once

#include "config/settings.hpp"
#include "ui/color.hpp"

#include <algorithm>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

namespace math_solver {

    // Computes the Levenshtein edit distance between two strings.
    //
    // - Assumes ASCII or single-byte characters; multi-byte encodings may yield
    //   incorrect results.
    // - O(m * n) time and O(n) space, where m and n are the string lengths.
    // - Used for fuzzy matching in user-facing suggestion logic.
    // - If performance becomes a bottleneck, consider bounded or early-exit
    // variants.
    inline int edit_distance(const std::string& a, const std::string& b) {
        const size_t     m = a.size(), n = b.size();
        std::vector<int> prev(n + 1), curr(n + 1);
        for (size_t j = 0; j <= n; ++j)
            prev[j] = static_cast<int>(j);
        for (size_t i = 1; i <= m; ++i) {
            curr[0] = static_cast<int>(i);
            for (size_t j = 1; j <= n; ++j) {
                int cost = (a[i - 1] == b[j - 1]) ? 0 : 1;
                curr[j]  = std::min(
                    {prev[j] + 1, curr[j - 1] + 1, prev[j - 1] + cost});
            }
            std::swap(prev, curr);
        }
        return prev[n];
    }

    // Returns the closest candidate to `input` within `max_distance`.
    //
    // - Returns std::nullopt if no candidate is within the threshold.
    // - If multiple candidates are equally close, returns the first
    // encountered.
    // - Used for "did you mean" diagnostics; not intended for bulk queries.
    inline std::optional<std::string>
    suggest(const std::string&              input,
            const std::vector<std::string>& candidates,
            int                             max_distance = 2) {
        std::optional<std::string> best;
        int                        best_dist = max_distance + 1;
        for (const auto& c : candidates) {
            int d = edit_distance(input, c);
            if (d < best_dist) {
                best_dist = d;
                best      = c;
            }
        }
        return best;
    }

    // Returns all candidates within `max_distance` of `input`, sorted by
    // distance.
    //
    // - Excludes exact matches (distance == 0).
    // - Sorting is stable with respect to candidate order for ties.
    // - Intended for batch suggestion UIs; not optimized for large candidate
    // sets.
    inline std::vector<std::string>
    suggest_all(const std::string&              input,
                const std::vector<std::string>& candidates,
                int                             max_distance = 2) {
        std::vector<std::pair<int, std::string>> matches;
        for (const auto& c : candidates) {
            int d = edit_distance(input, c);
            if (d <= max_distance && d > 0)
                matches.emplace_back(d, c);
        }
        std::sort(matches.begin(), matches.end());
        std::vector<std::string> result;
        for (auto& [_, c] : matches)
            result.push_back(std::move(c));
        return result;
    }

    // Emits a "did you mean" suggestion to stdout if a close match exists.
    //
    // - Used in user-facing diagnostics for typos or unknown identifiers.
    // - Relies on `ansi` formatting for output; assumes terminal supports ANSI
    // escapes.
    // - No output if no suitable suggestion is found.
    inline void maybe_suggest(const std::string&              word,
                              const std::vector<std::string>& candidates,
                              int max_distance = 2) {
        auto match = suggest(word, candidates, max_distance);
        if (match) {
            std::cout << ansi::dim << "  Did you mean " << ansi::reset
                      << ansi::bold << *match << ansi::reset << ansi::dim << "?"
                      << ansi::reset << "\n";
        }
    }

    // Specialized suggestion for settings keys.
    //
    // - Relies on Settings::all_keys() to enumerate valid keys.
    // - Used in diagnostics for unknown configuration keys.
    inline void maybe_suggest_setting(const std::string& word) {
        maybe_suggest(word, Settings::all_keys());
    }

    // Legacy aliases for command/subcommand suggestion.
    //
    // - Retained for compatibility with older code paths.
    // - Prefer using maybe_suggest directly for new code.
    inline void
    maybe_suggest_command(const std::string&              word,
                          const std::vector<std::string>& all_commands) {
        maybe_suggest(word, all_commands);
    }

    inline void maybe_suggest_subcommand(const std::string&              word,
                                         const std::vector<std::string>& subs) {
        maybe_suggest(word, subs);
    }

} // namespace math_solver