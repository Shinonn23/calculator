#ifndef FORMAT_UTILS_H
#define FORMAT_UTILS_H

#include <cmath>
#include <string>

namespace math_solver {

    // Central double formatting — strips trailing zeros from decimal
    // representation Used everywhere that displays numeric values to the user
    inline std::string format_double(double value) {
        // Snap near-zero values to exact zero
        if (std::abs(value) < 1e-12)
            value = 0.0;

        std::string str = std::to_string(value);
        size_t      dot = str.find('.');
        if (dot != std::string::npos) {
            str.erase(str.find_last_not_of('0') + 1, std::string::npos);
            if (str.back() == '.')
                str.pop_back();
        }
        return str;
    }

} // namespace math_solver

#endif
