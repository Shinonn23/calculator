#ifndef POLYNOMIAL_H
#define POLYNOMIAL_H

#include <algorithm>
#include <cmath>
#include <map>
#include <numeric>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace math_solver {

    // ========================================================================
    // Monomial — represents a product of variables raised to integer powers
    // e.g. x^2 * y^1  =>  {x:2, y:1}
    // ========================================================================
    class Monomial {
        private:
        std::map<std::string, int> vars_; // variable -> exponent

        public:
        Monomial() = default;

        explicit Monomial(const std::string& var, int exp = 1) {
            if (exp != 0)
                vars_[var] = exp;
        }

        explicit Monomial(const std::map<std::string, int>& vars) {
            for (const auto& [v, e] : vars) {
                if (e != 0)
                    vars_[v] = e;
            }
        }

        const std::map<std::string, int>& vars() const { return vars_; }

        int total_degree() const {
            int deg = 0;
            for (const auto& [_, e] : vars_)
                deg += e;
            return deg;
        }

        int degree_of(const std::string& var) const {
            auto it = vars_.find(var);
            return (it != vars_.end()) ? it->second : 0;
        }

        bool is_constant() const { return vars_.empty(); }

        // All variable names appearing in this monomial
        std::vector<std::string> variable_names() const {
            std::vector<std::string> names;
            for (const auto& [v, _] : vars_)
                names.push_back(v);
            return names;
        }

        // Multiply two monomials: add exponents
        Monomial operator*(const Monomial& other) const {
            Monomial result = *this;
            for (const auto& [v, e] : other.vars_) {
                result.vars_[v] += e;
                if (result.vars_[v] == 0)
                    result.vars_.erase(v);
            }
            return result;
        }

        // Divide monomial by another: subtract exponents
        // Returns nullopt if result has negative exponents
        Monomial operator/(const Monomial& other) const {
            Monomial result = *this;
            for (const auto& [v, e] : other.vars_) {
                result.vars_[v] -= e;
                if (result.vars_[v] == 0)
                    result.vars_.erase(v);
            }
            return result;
        }

        // Check if this monomial is divisible by another
        bool divisible_by(const Monomial& other) const {
            for (const auto& [v, e] : other.vars_) {
                auto it = vars_.find(v);
                if (it == vars_.end() || it->second < e)
                    return false;
            }
            return true;
        }

        // Raise monomial to a power
        Monomial pow(int n) const {
            if (n == 0)
                return Monomial();
            Monomial result;
            for (const auto& [v, e] : vars_) {
                result.vars_[v] = e * n;
            }
            return result;
        }

        // Comparison for use as map key
        // Graded lexicographic order:
        //   1. Higher total degree first
        //   2. Same total degree: higher power of lexicographically earlier
        //      variable comes first (e.g. x^2 < xy < y^2 in this ordering)
        bool operator<(const Monomial& other) const {
            int d1 = total_degree();
            int d2 = other.total_degree();
            if (d1 != d2)
                return d1 > d2; // higher degree first

            // Same total degree: compare variable exponents
            // Collect all variable names from both monomials
            std::set<std::string> all_vars;
            for (const auto& [v, _] : vars_)
                all_vars.insert(v);
            for (const auto& [v, _] : other.vars_)
                all_vars.insert(v);

            // Compare exponent of each variable in alphabetical order
            // Higher exponent of the earlier variable wins (comes first)
            for (const auto& v : all_vars) {
                int e1 = degree_of(v);
                int e2 = other.degree_of(v);
                if (e1 != e2)
                    return e1 > e2; // higher exponent of earlier var first
            }
            return false; // equal
        }

        bool operator==(const Monomial& other) const {
            return vars_ == other.vars_;
        }

        bool operator!=(const Monomial& other) const {
            return !(*this == other);
        }

        // Format monomial for display (without coefficient)
        // e.g. "x^2y" or "xy^2" or "" (for constants)
        std::string to_string() const {
            if (vars_.empty())
                return "";

            // Sort variables alphabetically for consistent output
            std::vector<std::pair<std::string, int>> sorted(vars_.begin(),
                                                            vars_.end());
            std::sort(sorted.begin(), sorted.end());

            std::string result;
            for (const auto& [v, e] : sorted) {
                result += v;
                if (e != 1) {
                    result += "^" + std::to_string(e);
                }
            }
            return result;
        }
    };

    // ========================================================================
    // Polynomial — a sum of (coefficient * monomial) terms
    // Stored as map<Monomial, double> — automatically combines like terms
    // ========================================================================
    class Polynomial {
        private:
        std::map<Monomial, double> terms_;

        // Remove zero-coefficient terms
        void cleanup() {
            for (auto it = terms_.begin(); it != terms_.end();) {
                if (std::abs(it->second) < 1e-12)
                    it = terms_.erase(it);
                else
                    ++it;
            }
        }

        public:
        Polynomial() = default;

        // Constant polynomial
        explicit Polynomial(double constant) {
            if (std::abs(constant) >= 1e-12)
                terms_[Monomial()] = constant;
        }

        // Single variable polynomial: coeff * var^exp
        Polynomial(double coeff, const std::string& var, int exp = 1) {
            if (std::abs(coeff) >= 1e-12)
                terms_[Monomial(var, exp)] = coeff;
        }

        // Single monomial term
        Polynomial(double coeff, const Monomial& mono) {
            if (std::abs(coeff) >= 1e-12)
                terms_[mono] = coeff;
        }

        const std::map<Monomial, double>& terms() const { return terms_; }

        bool is_zero() const { return terms_.empty(); }

        bool is_constant() const {
            if (terms_.empty())
                return true;
            return terms_.size() == 1 && terms_.begin()->first.is_constant();
        }

        double constant_value() const {
            if (terms_.empty())
                return 0;
            auto it = terms_.find(Monomial());
            return (it != terms_.end()) ? it->second : 0;
        }

        // Get coefficient of a specific monomial
        double coefficient(const Monomial& m) const {
            auto it = terms_.find(m);
            return (it != terms_.end()) ? it->second : 0.0;
        }

        size_t num_terms() const { return terms_.size(); }

        // Total degree of the polynomial
        int degree() const {
            int max_deg = 0;
            for (const auto& [m, _] : terms_)
                max_deg = std::max(max_deg, m.total_degree());
            return max_deg;
        }

        // All variable names in the polynomial
        std::vector<std::string> variables() const {
            std::map<std::string, bool> seen;
            for (const auto& [m, _] : terms_) {
                for (const auto& v : m.variable_names())
                    seen[v] = true;
            }
            std::vector<std::string> result;
            for (const auto& [v, _] : seen)
                result.push_back(v);
            return result;
        }

        // Check if the polynomial is univariate
        bool is_univariate() const { return variables().size() <= 1; }

        // Get the single variable name (only valid if univariate)
        std::string single_variable() const {
            auto vars = variables();
            return vars.empty() ? "x" : vars[0];
        }

        // Get coefficient of x^n for univariate polynomial
        double coeff_of_degree(int n) const {
            for (const auto& [m, c] : terms_) {
                if (m.total_degree() == n)
                    return c;
            }
            return 0.0;
        }

        // ====================================================================
        // Arithmetic operations
        // ====================================================================

        Polynomial operator+(const Polynomial& other) const {
            Polynomial result = *this;
            for (const auto& [m, c] : other.terms_)
                result.terms_[m] += c;
            result.cleanup();
            return result;
        }

        Polynomial operator-(const Polynomial& other) const {
            Polynomial result = *this;
            for (const auto& [m, c] : other.terms_)
                result.terms_[m] -= c;
            result.cleanup();
            return result;
        }

        Polynomial operator-() const {
            Polynomial result;
            for (const auto& [m, c] : terms_)
                result.terms_[m] = -c;
            return result;
        }

        Polynomial operator*(const Polynomial& other) const {
            Polynomial result;
            for (const auto& [m1, c1] : terms_) {
                for (const auto& [m2, c2] : other.terms_) {
                    Monomial product = m1 * m2;
                    result.terms_[product] += c1 * c2;
                }
            }
            result.cleanup();
            return result;
        }

        // Scalar multiplication
        Polynomial operator*(double scalar) const {
            if (std::abs(scalar) < 1e-12)
                return Polynomial();
            Polynomial result;
            for (const auto& [m, c] : terms_)
                result.terms_[m] = c * scalar;
            result.cleanup();
            return result;
        }

        // Division by scalar
        Polynomial operator/(double scalar) const {
            return *this * (1.0 / scalar);
        }

        // Power (non-negative integer)
        Polynomial pow(int n) const {
            if (n == 0)
                return Polynomial(1);
            if (n == 1)
                return *this;

            // Fast exponentiation
            Polynomial result(1);
            Polynomial base = *this;
            int        exp  = n;
            while (exp > 0) {
                if (exp % 2 == 1)
                    result = result * base;
                base = base * base;
                exp /= 2;
            }
            return result;
        }

        // ====================================================================
        // Factoring helpers
        // ====================================================================

        // GCD of all coefficients
        double coefficient_gcd() const {
            if (terms_.empty())
                return 1.0;

            // Check if all coefficients are integers
            bool all_integer = true;
            for (const auto& [_, c] : terms_) {
                if (std::abs(c - std::round(c)) > 1e-9) {
                    all_integer = false;
                    break;
                }
            }

            if (!all_integer)
                return 1.0;

            int64_t g = 0;
            for (const auto& [_, c] : terms_) {
                int64_t ic = static_cast<int64_t>(std::round(std::abs(c)));
                g          = std::gcd(g, ic);
            }
            return (g == 0) ? 1.0 : static_cast<double>(g);
        }

        // GCD monomial — the monomial that divides all terms
        Monomial monomial_gcd() const {
            if (terms_.empty())
                return Monomial();

            auto it = terms_.begin();
            std::map<std::string, int> common = it->first.vars();
            ++it;

            for (; it != terms_.end(); ++it) {
                const auto& vars = it->first.vars();
                // Keep only variables present in both, with min exponent
                for (auto cit = common.begin(); cit != common.end();) {
                    auto vit = vars.find(cit->first);
                    if (vit == vars.end()) {
                        cit = common.erase(cit);
                    } else {
                        cit->second = std::min(cit->second, vit->second);
                        if (cit->second <= 0)
                            cit = common.erase(cit);
                        else
                            ++cit;
                    }
                }
            }

            return Monomial(common);
        }

        // Divide every term by a monomial
        Polynomial divide_by_monomial(const Monomial& m) const {
            Polynomial result;
            for (const auto& [mono, c] : terms_) {
                result.terms_[mono / m] = c;
            }
            return result;
        }

        // ====================================================================
        // Output formatting
        // ====================================================================

        std::string to_string() const {
            if (terms_.empty())
                return "0";

            // Terms are already sorted by Monomial::operator<
            // (highest degree first, then lexicographic)

            std::string result;
            bool        first = true;

            for (const auto& [m, c] : terms_) {
                if (std::abs(c) < 1e-12)
                    continue;

                std::string mono_str = m.to_string();
                bool        has_vars = !mono_str.empty();

                if (first) {
                    if (has_vars) {
                        if (std::abs(c - 1.0) < 1e-9) {
                            result += mono_str;
                        } else if (std::abs(c + 1.0) < 1e-9) {
                            result += "-" + mono_str;
                        } else {
                            result += format_number(c) + mono_str;
                        }
                    } else {
                        result += format_number(c);
                    }
                    first = false;
                } else {
                    if (c > 0) {
                        result += " + ";
                        if (has_vars) {
                            if (std::abs(c - 1.0) < 1e-9) {
                                result += mono_str;
                            } else {
                                result += format_number(c) + mono_str;
                            }
                        } else {
                            result += format_number(c);
                        }
                    } else {
                        result += " - ";
                        double abs_c = std::abs(c);
                        if (has_vars) {
                            if (std::abs(abs_c - 1.0) < 1e-9) {
                                result += mono_str;
                            } else {
                                result += format_number(abs_c) + mono_str;
                            }
                        } else {
                            result += format_number(abs_c);
                        }
                    }
                }
            }

            return result.empty() ? "0" : result;
        }

        private:
        static std::string format_number(double val) {
            if (std::abs(val - std::round(val)) < 1e-9) {
                return std::to_string(static_cast<int64_t>(std::round(val)));
            }
            std::string str = std::to_string(val);
            str.erase(str.find_last_not_of('0') + 1, std::string::npos);
            if (str.back() == '.')
                str.pop_back();
            return str;
        }
    };

} // namespace math_solver

#endif
