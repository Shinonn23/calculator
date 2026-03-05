#pragma once

//! # Module — `src/algebra/polynomial/polynomial.hpp`
//!
//! Defines `Monomial` and `Polynomial` — the core symbolic algebra types used
//! throughout the algebra layer. `Monomial` represents a single power product
//! of variables (e.g. `x²y`); `Polynomial` is a finite sum of
//! coefficient–monomial pairs. Both types maintain canonical form: zero
//! exponents and zero coefficients are always pruned after every mutation.
//!
//! Consumed by `ASTToPolynomial`, `factor_polynomial`, and `PolynomialSolver`.

#include "core/tolerance.hpp"
#include <algorithm>
#include <cmath>
#include <map>
#include <numeric>
#include <set>
#include <string>
#include <vector>

namespace math_solver {

    /// Models a product of variables with non-negative integer exponents.
    ///
    /// Stored as a `std::map<string, int>` from variable name to exponent.
    /// Zero exponents are never kept — they are pruned on every mutation.
    /// The empty map represents the constant monomial 1.
    ///
    /// Variable names are case-sensitive and must be stable across the
    /// codebase. The type is used as a `std::map` key; `operator<` implements
    /// graded lexicographic order (total degree descending, then per-variable
    /// exponent descending) to ensure a deterministic canonical ordering.
    class Monomial {
        private:
        std::map<std::string, int> vars_;

        public:
        /// Constructs the constant monomial 1 (empty variable map).
        Monomial() = default;

        /// Constructs the monomial `var^exp`.
        ///
        /// # Arguments
        ///
        /// * `var` — Variable name; must be non-empty.
        /// * `exp` — Exponent; if zero the result is the constant monomial 1.
        explicit Monomial(const std::string& var, int exp = 1) {
            if (exp != 0)
                vars_[var] = exp;
        }

        /// Constructs a monomial from a map of variable-to-exponent entries.
        ///
        /// Entries with a zero exponent are silently dropped.
        ///
        /// # Arguments
        ///
        /// * `vars` — Map of variable names to their integer exponents.
        explicit Monomial(const std::map<std::string, int>& vars) {
            for (const auto& [v, e] : vars) {
                if (e != 0)
                    vars_[v] = e;
            }
        }

        /// Returns the internal variable-to-exponent map.
        const std::map<std::string, int>& vars() const { return vars_; }

        /// Returns the sum of all exponents (total degree).
        int                               total_degree() const {
            int deg = 0;
            for (const auto& [_, e] : vars_)
                deg += e;
            return deg;
        }

        /// Returns the exponent of `var`, or 0 if `var` is not present.
        int degree_of(const std::string& var) const {
            auto it = vars_.find(var);
            return (it != vars_.end()) ? it->second : 0;
        }

        /// Returns `true` if this is the constant monomial 1 (no variables).
        bool                     is_constant() const { return vars_.empty(); }

        /// Returns the sorted list of variable names appearing in this monomial.
        std::vector<std::string> variable_names() const {
            std::vector<std::string> names;
            for (const auto& [v, _] : vars_)
                names.push_back(v);
            return names;
        }

        /// Multiplies this monomial by `other`, merging exponents.
        ///
        /// Variables whose resulting exponent becomes zero are pruned.
        Monomial operator*(const Monomial& other) const {
            Monomial result = *this;
            for (const auto& [v, e] : other.vars_) {
                result.vars_[v] += e;
                if (result.vars_[v] == 0)
                    result.vars_.erase(v);
            }
            return result;
        }

        /// Divides this monomial by `other`, subtracting exponents.
        ///
        /// Variables whose resulting exponent becomes zero are pruned. The
        /// caller is responsible for ensuring divisibility; negative exponents
        /// may result if `divisible_by` is not checked first.
        Monomial operator/(const Monomial& other) const {
            Monomial result = *this;
            for (const auto& [v, e] : other.vars_) {
                result.vars_[v] -= e;
                if (result.vars_[v] == 0)
                    result.vars_.erase(v);
            }
            return result;
        }

        /// Returns `true` if every exponent in `other` is ≤ the corresponding
        /// exponent in `*this` (i.e. exact monomial division is possible).
        bool divisible_by(const Monomial& other) const {
            for (const auto& [v, e] : other.vars_) {
                auto it = vars_.find(v);
                if (it == vars_.end() || it->second < e)
                    return false;
            }
            return true;
        }

        /// Returns this monomial raised to the non-negative integer power `n`.
        ///
        /// Returns the constant monomial 1 when `n == 0`.
        Monomial pow(int n) const {
            if (n == 0)
                return Monomial();
            Monomial result;
            for (const auto& [v, e] : vars_) {
                result.vars_[v] = e * n;
            }
            return result;
        }

        /// Compares monomials in graded lexicographic order.
        ///
        /// Higher total degree sorts first. Among equal-degree monomials the
        /// one with a larger exponent on the first differing variable sorts
        /// first. This order is consistent and total, making `Monomial`
        /// suitable as a `std::map` key.
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

        /// Returns `true` if both monomials have identical variable maps.
        bool operator==(const Monomial& other) const {
            return vars_ == other.vars_;
        }

        /// Returns `true` if the monomials differ in any variable or exponent.
        bool operator!=(const Monomial& other) const {
            return !(*this == other);
        }

        /// Returns a compact human-readable string (e.g. `"x^2y"`).
        ///
        /// Variables are sorted alphabetically. Exponent 1 is suppressed.
        /// Returns `""` for the constant monomial 1.
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

    /// Sum of `(coefficient × monomial)` terms with `double` coefficients.
    ///
    /// Terms are stored in a `std::map<Monomial, double>` keyed by the
    /// monomial's canonical order, so the polynomial is always in graded
    /// lexicographic form. Zero-coefficient terms are pruned after every
    /// arithmetic operation via `cleanup()`.
    ///
    /// All arithmetic operators return new `Polynomial` instances; the object
    /// itself is never mutated by operator calls.
    class Polynomial {
        private:
        std::map<Monomial, double> terms_;

        /// Removes any term whose absolute coefficient is below `kEpsilon`.
        void                       cleanup() {
            for (auto it = terms_.begin(); it != terms_.end();) {
                if (std::abs(it->second) < kEpsilon)
                    it = terms_.erase(it);
                else
                    ++it;
            }
        }

        public:
        /// Constructs the zero polynomial.
        Polynomial() = default;

        /// Constructs the constant polynomial `constant`.
        ///
        /// If `|constant| < kEpsilon`, the result is the zero polynomial.
        explicit Polynomial(double constant) {
            if (std::abs(constant) >= kEpsilon)
                terms_[Monomial()] = constant;
        }

        /// Constructs the single-term polynomial `coeff * var^exp`.
        ///
        /// If `|coeff| < kEpsilon`, the result is the zero polynomial.
        Polynomial(double coeff, const std::string& var, int exp = 1) {
            if (std::abs(coeff) >= kEpsilon)
                terms_[Monomial(var, exp)] = coeff;
        }

        /// Constructs the single-term polynomial `coeff * mono`.
        ///
        /// If `|coeff| < kEpsilon`, the result is the zero polynomial.
        Polynomial(double coeff, const Monomial& mono) {
            if (std::abs(coeff) >= kEpsilon)
                terms_[mono] = coeff;
        }

        /// Returns the internal term map (monomial → coefficient).
        const std::map<Monomial, double>& terms() const { return terms_; }

        /// Returns `true` if the polynomial is identically zero (no terms).
        bool is_zero() const { return terms_.empty(); }

        /// Returns `true` if the polynomial is a constant (degree 0 or zero).
        bool is_constant() const {
            if (terms_.empty())
                return true;
            return terms_.size() == 1 && terms_.begin()->first.is_constant();
        }

        /// Returns the value of the constant term, or `0.0` if absent.
        double constant_value() const {
            if (terms_.empty())
                return 0;
            auto it = terms_.find(Monomial());
            return (it != terms_.end()) ? it->second : 0;
        }

        /// Returns the coefficient of monomial `m`, or `0.0` if absent.
        double coefficient(const Monomial& m) const {
            auto it = terms_.find(m);
            return (it != terms_.end()) ? it->second : 0.0;
        }

        /// Returns the number of non-zero terms.
        size_t num_terms() const { return terms_.size(); }

        /// Returns the maximal total degree over all terms, or 0 for the zero
        /// polynomial.
        int    degree() const {
            int max_deg = 0;
            for (const auto& [m, _] : terms_)
                max_deg = std::max(max_deg, m.total_degree());
            return max_deg;
        }

        /// Returns all variable names that appear in any term, sorted
        /// alphabetically.
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

        /// Returns `true` if the polynomial contains at most one distinct
        /// variable.
        bool        is_univariate() const { return variables().size() <= 1; }

        /// Returns the name of the single variable, or `"x"` if there are none.
        std::string single_variable() const {
            auto vars = variables();
            return vars.empty() ? "x" : vars[0];
        }

        /// Returns the coefficient of the term with total degree `n`, or `0.0`
        /// if no such term exists.
        double coeff_of_degree(int n) const {
            for (const auto& [m, c] : terms_) {
                if (m.total_degree() == n)
                    return c;
            }
            return 0.0;
        }

        /// Returns the sum of this polynomial and `other`.
        Polynomial operator+(const Polynomial& other) const {
            Polynomial result = *this;
            for (const auto& [m, c] : other.terms_)
                result.terms_[m] += c;
            result.cleanup();
            return result;
        }

        /// Returns the difference of this polynomial and `other`.
        Polynomial operator-(const Polynomial& other) const {
            Polynomial result = *this;
            for (const auto& [m, c] : other.terms_)
                result.terms_[m] -= c;
            result.cleanup();
            return result;
        }

        /// Returns the negation of this polynomial.
        Polynomial operator-() const {
            Polynomial result;
            for (const auto& [m, c] : terms_)
                result.terms_[m] = -c;
            return result;
        }

        /// Returns the product of this polynomial and `other`.
        ///
        /// Distributes over all pairs of terms in O(n·m) time where n and m
        /// are the respective term counts.
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

        /// Returns this polynomial scaled by `scalar`.
        ///
        /// Returns the zero polynomial if `|scalar| < kEpsilon`.
        Polynomial operator*(double scalar) const {
            if (std::abs(scalar) < kEpsilon)
                return Polynomial();
            Polynomial result;
            for (const auto& [m, c] : terms_)
                result.terms_[m] = c * scalar;
            result.cleanup();
            return result;
        }

        /// Returns this polynomial divided by the scalar `scalar`.
        ///
        /// Equivalent to `*this * (1.0 / scalar)`. The caller is responsible
        /// for ensuring `scalar` is non-zero.
        Polynomial operator/(double scalar) const {
            return *this * (1.0 / scalar);
        }

        /// Returns this polynomial raised to the non-negative integer power `n`.
        ///
        /// Uses binary exponentiation. Returns the constant polynomial 1 when
        /// `n == 0`.
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

        /// Returns the GCD of all coefficients when all are integer-valued,
        /// or `1.0` if any coefficient is non-integer.
        ///
        /// Used during factorization to extract a scalar GCD from a primitive
        /// polynomial.
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

        /// Returns the GCD monomial of all terms (intersection of variable
        /// exponents, taking the minimum per variable).
        ///
        /// Used during factorization to extract the monomial factor common to
        /// every term.
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

        /// Returns a new polynomial with every term divided by monomial `m`.
        ///
        /// Equivalent to subtracting `m`'s exponents from each term's monomial.
        /// The caller must ensure `m` divides every term (use `monomial_gcd`).
        Polynomial divide_by_monomial(const Monomial& m) const {
            Polynomial result;
            for (const auto& [mono, c] : terms_) {
                result.terms_[mono / m] = c;
            }
            return result;
        }

        /// Returns a human-readable string representation (e.g. `"x^2 + 2x -
        /// 3"`).
        ///
        /// Not guaranteed to be re-parseable. Coefficients of ±1 on
        /// non-constant terms are suppressed. Returns `"0"` for the zero
        /// polynomial.
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
        /// Formats `val` as a decimal string with trailing zeros stripped.
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
