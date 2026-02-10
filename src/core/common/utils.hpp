#ifndef UTILS_H
#define UTILS_H

#include <cmath>
#include <iomanip>
#include <sstream>
#include <string>

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

} // namespace math_solver

#endif
