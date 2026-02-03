#ifndef FRACTION_H
#define FRACTION_H

#include <cmath>
#include <cstdint>
#include <string>
#include <utility>

namespace math_solver {

    struct Fraction {
        int64_t numerator;
        int64_t denominator;

        Fraction(int64_t num = 0, int64_t den = 1)
            : numerator(num), denominator(den) {
            if (denominator < 0) {
                numerator = -numerator;
                denominator = -denominator;
            }
            simplify();
        }

        void simplify() {
            if (denominator == 0) {
                numerator = 0;
                denominator = 1;
                return;
            }
            int64_t g = gcd(std::abs(numerator), std::abs(denominator));
            if (g > 0) {
                numerator /= g;
                denominator /= g;
            }
        }

        std::string to_string() const {
            if (denominator == 1) {
                return std::to_string(numerator);
            }
            return std::to_string(numerator) + "/" + std::to_string(denominator);
        }

        double to_double() const {
            return static_cast<double>(numerator) / static_cast<double>(denominator);
        }

        bool is_integer() const {
            return denominator == 1;
        }

    private:
        static int64_t gcd(int64_t a, int64_t b) {
            while (b != 0) {
                int64_t t = b;
                b = a % b;
                a = t;
            }
            return a;
        }
    };

    // Convert double to fraction using continued fractions algorithm
    // Returns a fraction that approximates the value within tolerance
    // max_denominator limits the denominator to avoid huge fractions
    inline Fraction double_to_fraction(double value, double tolerance = 1e-9,
                                       int64_t max_denominator = 10000) {
        if (std::isnan(value) || std::isinf(value)) {
            return Fraction(0, 1);
        }

        bool negative = value < 0;
        value = std::abs(value);

        // Check if it's close to an integer
        double rounded = std::round(value);
        if (std::abs(value - rounded) < tolerance) {
            return Fraction(negative ? -static_cast<int64_t>(rounded)
                                     : static_cast<int64_t>(rounded), 1);
        }

        // Continued fractions algorithm
        int64_t h0 = 0, h1 = 1;
        int64_t k0 = 1, k1 = 0;
        double x = value;

        while (true) {
            int64_t a = static_cast<int64_t>(std::floor(x));
            int64_t h2 = a * h1 + h0;
            int64_t k2 = a * k1 + k0;

            if (k2 > max_denominator) {
                break;
            }

            h0 = h1; h1 = h2;
            k0 = k1; k1 = k2;

            double approx = static_cast<double>(h1) / static_cast<double>(k1);
            if (std::abs(approx - value) < tolerance) {
                break;
            }

            double remainder = x - a;
            if (std::abs(remainder) < tolerance) {
                break;
            }
            x = 1.0 / remainder;

            // Prevent infinite loop for irrational numbers
            if (x > 1e10) {
                break;
            }
        }

        return Fraction(negative ? -h1 : h1, k1);
    }

    // Format a coefficient for display
    // show_one: whether to show "1" explicitly (e.g., "1x" vs "x")
    // as_fraction: whether to display as fraction
    inline std::string format_coefficient(double coeff, bool show_one = false,
                                          bool as_fraction = false) {
        if (!as_fraction) {
            // Decimal format
            if (!show_one && std::abs(coeff - 1.0) < 1e-9) {
                return "";
            }
            if (!show_one && std::abs(coeff + 1.0) < 1e-9) {
                return "-";
            }

            // Format nicely without trailing zeros
            std::string str = std::to_string(coeff);
            size_t dot_pos = str.find('.');
            if (dot_pos != std::string::npos) {
                str.erase(str.find_last_not_of('0') + 1, std::string::npos);
                if (str.back() == '.') {
                    str.pop_back();
                }
            }
            return str;
        }

        // Fraction format
        Fraction frac = double_to_fraction(coeff);

        if (!show_one && frac.numerator == 1 && frac.denominator == 1) {
            return "";
        }
        if (!show_one && frac.numerator == -1 && frac.denominator == 1) {
            return "-";
        }

        if (frac.denominator == 1) {
            return std::to_string(frac.numerator);
        }

        return "(" + frac.to_string() + ")";
    }

} // namespace math_solver

#endif
