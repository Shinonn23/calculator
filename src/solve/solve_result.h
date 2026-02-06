#ifndef SOLVE_RESULT_H
#define SOLVE_RESULT_H

#include "common/format_utils.h"
#include <string>
#include <vector>

namespace math_solver {

    // ------------------------------------------------------------------
    // SolveFlags: post-solve root filter requested by the user.
    //   e.g.  "solve positive x^2 - 4 = 0"  →  SolveFlags::Positive
    // ------------------------------------------------------------------
    enum class SolveFlags {
        All,      // no filter (default)
        Positive, // keep only values > 0
        Negative, // keep only values < 0
        NonNeg,   // keep only values >= 0
        Integer,  // keep only integer values
    };

    // Parse a flag keyword ("positive", "negative", "nonneg", "integer")
    // Returns SolveFlags::All if unrecognised.
    inline SolveFlags parse_solve_flag(const std::string& word) {
        if (word == "positive")
            return SolveFlags::Positive;
        if (word == "negative")
            return SolveFlags::Negative;
        if (word == "nonneg" || word == "nonnegative")
            return SolveFlags::NonNeg;
        if (word == "integer" || word == "int")
            return SolveFlags::Integer;
        return SolveFlags::All;
    }

    // Apply a SolveFlags filter to a vector of values in-place.
    // Returns the count of values removed.
    inline size_t apply_solve_flags(std::vector<double>& values,
                                    SolveFlags           flag) {
        if (flag == SolveFlags::All || values.empty())
            return 0;

        size_t              before = values.size();
        std::vector<double> kept;
        kept.reserve(before);
        for (double v : values) {
            bool pass = true;
            switch (flag) {
            case SolveFlags::Positive:
                pass = v > 1e-12;
                break;
            case SolveFlags::Negative:
                pass = v < -1e-12;
                break;
            case SolveFlags::NonNeg:
                pass = v >= -1e-12;
                break;
            case SolveFlags::Integer:
                pass = std::abs(v - std::round(v)) < 1e-9;
                break;
            default:
                break;
            }
            if (pass)
                kept.push_back(v);
        }
        values = std::move(kept);
        return before - values.size();
    }

    // ------------------------------------------------------------------
    // SolveResult: the output of solving an equation.
    // ------------------------------------------------------------------
    struct SolveResult {
        std::string         variable;     // The variable we solved for
        std::vector<double> values;       // The solution(s)
        bool                has_solution; // True if at least one solution

        // Convenience: get first (or only) solution value
        double      value() const { return values.empty() ? 0.0 : values[0]; }

        std::string to_string() const {
            if (!has_solution || values.empty()) {
                return "no solution";
            }
            if (values.size() == 1) {
                return variable + " = " + format_double(values[0]);
            }
            // Multiple solutions
            std::string result = variable + " = [";
            for (size_t i = 0; i < values.size(); ++i) {
                if (i > 0)
                    result += ", ";
                result += format_double(values[i]);
            }
            result += "]";
            return result;
        }
    };

} // namespace math_solver

#endif // SOLVE_RESULT_H
