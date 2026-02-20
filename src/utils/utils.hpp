#pragma once

#include <cmath>
#include <iomanip>
#include <sstream>
#include <string>

namespace math_solver {

    // Formats a floating-point value for human-readable output, with the
    // following invariants:
    // - Ensures at most `precision` decimal places (default: 6).
    // - Strips trailing zeros to avoid misleading precision in diagnostics or
    // logs.
    // - Canonicalizes "-0" to "0" to avoid negative zero confusion in
    // downstream consumers.
    // - Handles NaN and infinities explicitly to avoid locale-dependent or
    // implementation-defined output.
    // - This is intended for display, not for serialization or round-tripping.
    // - Performance: relies on std::ostringstream, which is not optimal for hot
    // paths.
    inline std::string format_double(double value, int precision = 6) {
        if (std::isnan(value))
            return "NaN";
        if (std::isinf(value))
            return value > 0 ? "Infinity" : "-Infinity";

        std::ostringstream oss;
        oss << std::fixed << std::setprecision(precision) << value;
        std::string s   = oss.str();

        size_t      dot = s.find('.');
        if (dot != std::string::npos) {
            // Remove trailing zeros after the decimal point, but preserve at
            // least one digit if needed.
            size_t last_nonzero = s.find_last_not_of('0');
            if (last_nonzero != std::string::npos && last_nonzero > dot) {
                s.erase(last_nonzero + 1);
            } else if (last_nonzero == dot) {
                s.erase(dot);
            }
        }

        if (s == "-0")
            s = "0";

        return s;
    }

} // namespace math_solver
