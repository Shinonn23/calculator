#pragma once

#include "core/tolerance.hpp"
#include <algorithm>
#include <cmath>
#include <map>
#include <numeric>
#include <set>
#include <string>
#include <vector>

namespace math_solver {

    // Monomial: models a product of variables with integer exponents.
    //
    // Invariants:
    // - Exponents are always nonzero; zero exponents are omitted from vars_.
    // - The empty map denotes the constant monomial 1.
    //
    // Correctness relies on:
    // - All algebraic manipulations must preserve the nonzero-exponent
    // invariant.
    // - Used as a key in maps; operator< must be consistent and total.
    // - Variable names are case-sensitive and must be stable across the
    // codebase.
    //
    // Performance: vars_ is a std::map, so monomial operations are O(n log n)
    // in the number of variables.
    class Monomial {
        private:
        std::map<std::string, int> vars_;

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

        int                               total_degree() const {
            int deg = 0;
            for (const auto& [_, e] : vars_)
                deg += e;
            return deg;
        }

        int degree_of(const std::string& var) const {
            auto it = vars_.find(var);
            return (it != vars_.end()) ? it->second : 0;
        }

        bool                     is_constant() const { return vars_.empty(); }

        std::vector<std::string> variable_names() const {
            std::vector<std::string> names;
            for (const auto& [v, _] : vars_)
                names.push_back(v);
            return names;
        }

        // Multiplication merges exponents; zero exponents are pruned.
        Monomial operator*(const Monomial& other) const {
            Monomial result = *this;
            for (const auto& [v, e] : other.vars_) {
                result.vars_[v] += e;
                if (result.vars_[v] == 0)
                    result.vars_.erase(v);
            }
            return result;
        }

        // Division subtracts exponents; zero exponents are pruned.
        Monomial operator/(const Monomial& other) const {
            Monomial result = *this;
            for (const auto& [v, e] : other.vars_) {
                result.vars_[v] -= e;
                if (result.vars_[v] == 0)
                    result.vars_.erase(v);
            }
            return result;
        }

        // Returns true if this monomial is divisible by 'other' (all exponents
        // >=).
        bool divisible_by(const Monomial& other) const {
            for (const auto& [v, e] : other.vars_) {
                auto it = vars_.find(v);
                if (it == vars_.end() || it->second < e)
                    return false;
            }
            return true;
        }

        // Exponentiation: raises all exponents to n.
        Monomial pow(int n) const {
            if (n == 0)
                return Monomial();
            Monomial result;
            for (const auto& [v, e] : vars_) {
                result.vars_[v] = e * n;
            }
            return result;
        }

        // Ordering: graded lexicographic (degree first, then variable order).
        // Used for canonicalization and as map key.
        bool operator<(const Monomial& other) const {
            int d1 = total_degree();
            int d2 = other.total_degree();
            if (d1 != d2)
                return d1 > d2;

            std::set<std::string> all_vars;
            for (const auto& [v, _] : vars_)
                all_vars.insert(v);
            for (const auto& [v, _] : other.vars_)
                all_vars.insert(v);

            for (const auto& v : all_vars) {
                int e1 = degree_of(v);
                int e2 = other.degree_of(v);
                if (e1 != e2)
                    return e1 > e2;
            }
            return false;
        }

        bool operator==(const Monomial& other) const {
            return vars_ == other.vars_;
        }

        bool operator!=(const Monomial& other) const {
            return !(*this == other);
        }

        // Returns a compact string representation; used for diagnostics and
        // pretty-printing.
        std::string to_string() const {
            if (vars_.empty())
                return "";

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

    // Polynomial: sum of (coefficient * monomial) terms.
    //
    // Invariants:
    // - Zero coefficients are pruned from terms_.
    // - terms_ is always sorted by Monomial::operator<.
    //
    // Correctness relies on:
    // - All algebraic operations must maintain canonical form (no zero terms,
    // no duplicate monomials).
    // - Coefficients are double; beware of floating-point precision when
    // checking for zero.
    // - Used for symbolic manipulation; correctness of term merging is
    // critical.
    //
    // Performance: All term lookups and insertions are O(log n) in the number
    // of terms.
    class Polynomial {
        private:
        std::map<Monomial, double> terms_;

        // Removes zero-coefficient terms. Called after all mutating operations.
        void                       cleanup() {
            for (auto it = terms_.begin(); it != terms_.end();) {
                if (std::abs(it->second) < kEpsilon)
                    it = terms_.erase(it);
                else
                    ++it;
            }
        }

        public:
        Polynomial() = default;

        explicit Polynomial(double constant) {
            if (std::abs(constant) >= kEpsilon)
                terms_[Monomial()] = constant;
        }

        Polynomial(double coeff, const std::string& var, int exp = 1) {
            if (std::abs(coeff) >= kEpsilon)
                terms_[Monomial(var, exp)] = coeff;
        }

        Polynomial(double coeff, const Monomial& mono) {
            if (std::abs(coeff) >= kEpsilon)
                terms_[mono] = coeff;
        }

        const std::map<Monomial, double>& terms() const { return terms_; }

        bool is_zero() const { return terms_.empty(); }

        // Returns true if the polynomial is a constant (possibly zero).
        bool is_constant() const {
            if (terms_.empty())
                return true;
            return terms_.size() == 1 && terms_.begin()->first.is_constant();
        }

        // Returns the constant term, or zero if absent.
        double constant_value() const {
            if (terms_.empty())
                return 0;
            auto it = terms_.find(Monomial());
            return (it != terms_.end()) ? it->second : 0;
        }

        // Returns the coefficient for a given monomial, or zero if absent.
        double coefficient(const Monomial& m) const {
            auto it = terms_.find(m);
            return (it != terms_.end()) ? it->second : 0.0;
        }

        size_t num_terms() const { return terms_.size(); }

        // Returns the maximal total degree among all terms.
        int    degree() const {
            int max_deg = 0;
            for (const auto& [m, _] : terms_)
                max_deg = std::max(max_deg, m.total_degree());
            return max_deg;
        }

        // Returns all variable names present in any term.
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

        // Returns true if the polynomial involves at most one variable.
        bool        is_univariate() const { return variables().size() <= 1; }

        // Returns the name of the single variable, or "x" if none.
        std::string single_variable() const {
            auto vars = variables();
            return vars.empty() ? "x" : vars[0];
        }

        // Returns the coefficient of the term with total degree n, or zero if
        // absent.
        double coeff_of_degree(int n) const {
            for (const auto& [m, c] : terms_) {
                if (m.total_degree() == n)
                    return c;
            }
            return 0.0;
        }

        // Addition: merges terms, combining like monomials.
        Polynomial operator+(const Polynomial& other) const {
            Polynomial result = *this;
            for (const auto& [m, c] : other.terms_)
                result.terms_[m] += c;
            result.cleanup();
            return result;
        }

        // Subtraction: merges terms, combining like monomials.
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

        // Multiplication: distributes over all pairs of terms.
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

        // Scalar multiplication.
        Polynomial operator*(double scalar) const {
            if (std::abs(scalar) < kEpsilon)
                return Polynomial();
            Polynomial result;
            for (const auto& [m, c] : terms_)
                result.terms_[m] = c * scalar;
            result.cleanup();
            return result;
        }

        Polynomial operator/(double scalar) const {
            return *this * (1.0 / scalar);
        }

        // Exponentiation by repeated squaring.
        Polynomial pow(int n) const {
            if (n == 0)
                return Polynomial(1);
            if (n == 1)
                return *this;

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

        // Returns the GCD of all coefficients, if all are integral; otherwise
        // returns 1.0.
        double coefficient_gcd() const {
            if (terms_.empty())
                return 1.0;

            bool all_integer = true;
            for (const auto& [_, c] : terms_) {
                if (std::abs(c - std::round(c)) > kCoeffTol) {
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

        // Returns the GCD of all monomials (intersection of variable
        // exponents).
        Monomial monomial_gcd() const {
            if (terms_.empty())
                return Monomial();

            auto                       it     = terms_.begin();
            std::map<std::string, int> common = it->first.vars();
            ++it;

            for (; it != terms_.end(); ++it) {
                const auto& vars = it->first.vars();
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

        // Divides all terms by the given monomial (exponents are subtracted).
        Polynomial divide_by_monomial(const Monomial& m) const {
            Polynomial result;
            for (const auto& [mono, c] : terms_) {
                result.terms_[mono / m] = c;
            }
            return result;
        }

        // Returns a human-readable string; not guaranteed to be parseable.
        std::string to_string() const {
            if (terms_.empty())
                return "0";

            std::string result;
            bool        first = true;

            for (const auto& [m, c] : terms_) {
                if (std::abs(c) < kEpsilon)
                    continue;

                std::string mono_str = m.to_string();
                bool        has_vars = !mono_str.empty();

                if (first) {
                    if (has_vars) {
                        if (std::abs(c - 1.0) < kCoeffTol) {
                            result += mono_str;
                        } else if (std::abs(c + 1.0) < kCoeffTol) {
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
                            if (std::abs(c - 1.0) < kCoeffTol) {
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
                            if (std::abs(abs_c - 1.0) < kCoeffTol) {
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
        // Formats a double as a string, trimming trailing zeros.
        static std::string format_number(double val) {
            if (std::abs(val - std::round(val)) < kCoeffTol) {
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
