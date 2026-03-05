#pragma once

//! # Module — `src/algebra/polynomial/factor.hpp`
//!
//! Polynomial factorization for the algebra layer. Provides `FactoredForm` —
//! the result type — and two entry points: `try_factor_quadratic` for integer
//! quadratics and `factor_polynomial` for the general case.
//!
//! Current coverage:
//! - Monomial GCD extraction (all degrees, all variable counts).
//! - Coefficient GCD extraction (primitive polynomials only).
//! - Full factorization of univariate degree-2 polynomials with integer
//!   coefficients into linear factors.
//! - Higher-degree and multivariate polynomials are returned unfactored.

#include "core/tolerance.hpp"
#include "polynomial.hpp"
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace math_solver {

    /// The result of factoring a polynomial.
    ///
    /// The factored form is:
    /// ```
    ///   numeric_factor * common_monomial * product_i( factors[i].first ^
    ///   factors[i].second )
    /// ```
    ///
    /// Invariant: `is_trivial()` is true iff no factorization was found — the
    /// single entry in `factors` is the original polynomial.
    struct FactoredForm {
        /// Scalar coefficient extracted from the polynomial (includes sign).
        double                                  numeric_factor = 1.0;

        /// Monomial GCD extracted from every term (e.g. `x` from `x² + x`).
        Monomial                                common_monomial;

        /// List of irreducible polynomial factors paired with their exponent.
        std::vector<std::pair<Polynomial, int>> factors;

        /// Returns `true` if no actual factorization occurred.
        ///
        /// Specifically, returns `true` when `numeric_factor == 1`,
        /// `common_monomial` is the constant 1, and there is exactly one
        /// factor with exponent 1.
        bool                                    is_trivial() const {
            return std::abs(numeric_factor - 1.0) < kCoeffTol &&
                   common_monomial.is_constant() && factors.size() == 1 &&
                   factors[0].second == 1;
        }

        /// Renders the factored form as a human-readable string.
        ///
        /// When `is_trivial()` is true, returns the single factor's string.
        /// Otherwise assembles `±N * mono * (f1)^e1 * (f2)^e2 …`, suppressing
        /// a leading `1` numeric factor and omitting exponents equal to 1.
        std::string to_string() const {
            if (is_trivial()) {
                return factors[0].first.to_string();
            }

            std::string result;

            bool        has_numeric = std::abs(numeric_factor - 1.0) > kCoeffTol &&
                               std::abs(numeric_factor + 1.0) > kCoeffTol;
            bool is_negative = numeric_factor < 0;

            // Special-case: output "-1" for negative unit with no other
            // factors.
            if (std::abs(numeric_factor + 1.0) < kCoeffTol &&
                common_monomial.is_constant() && factors.empty()) {
                return "-1";
            }

            if (has_numeric) {
                double abs_val = std::abs(numeric_factor);
                if (is_negative)
                    result += "-";
                if (std::abs(abs_val - std::round(abs_val)) < kCoeffTol) {
                    result += std::to_string(
                        static_cast<int64_t>(std::round(abs_val)));
                } else {
                    std::string s = std::to_string(abs_val);
                    s.erase(s.find_last_not_of('0') + 1, std::string::npos);
                    if (s.back() == '.')
                        s.pop_back();
                    result += s;
                }
            } else if (is_negative) {
                result += "-";
            }

            std::string mono_str = common_monomial.to_string();
            if (!mono_str.empty()) {
                result += mono_str;
            }

            for (const auto& [poly, exp] : factors) {
                std::string pstr = poly.to_string();
                result += "(" + pstr + ")";
                if (exp > 1) {
                    result += "^" + std::to_string(exp);
                }
            }

            return result.empty() ? "0" : result;
        }
    };

    /// Attempts to factor a degree-2 univariate polynomial with integer
    /// coefficients into two linear factors over the integers.
    ///
    /// Uses exhaustive divisor enumeration over divisors of the leading
    /// coefficient `a` and constant term `c`. The search is O(√|a| · √|c|).
    ///
    /// # Arguments
    ///
    /// * `poly`    — Univariate polynomial; must be exactly degree 2.
    /// * `var`     — The name of the single variable.
    /// * `factors` — Output parameter; receives the two linear factors (or one
    ///               squared factor) if factorization succeeds.
    ///
    /// # Returns
    ///
    /// `true` if integer linear factors were found and appended to `factors`;
    /// `false` if any coefficient is non-integer, the discriminant is negative,
    /// or the discriminant is not a perfect square.
    inline bool
    try_factor_quadratic(const Polynomial&                        poly,
                         const std::string&                       var,
                         std::vector<std::pair<Polynomial, int>>& factors) {

        double a = poly.coeff_of_degree(2);
        double b = poly.coeff_of_degree(1);
        double c = poly.coeff_of_degree(0);

        if (std::abs(a - std::round(a)) > kCoeffTol ||
            std::abs(b - std::round(b)) > kCoeffTol ||
            std::abs(c - std::round(c)) > kCoeffTol) {
            return false;
        }

        int64_t ia = static_cast<int64_t>(std::round(a));
        int64_t ib = static_cast<int64_t>(std::round(b));
        int64_t ic = static_cast<int64_t>(std::round(c));

        if (ia == 0)
            return false;

        int64_t disc = ib * ib - 4 * ia * ic;
        if (disc < 0)
            return false;

        int64_t sqrt_disc = static_cast<int64_t>(
            std::round(std::sqrt(static_cast<double>(std::abs(disc)))));
        if (sqrt_disc * sqrt_disc != disc)
            return false;

        // The following search is exhaustive over all possible integer
        // factorizations of the quadratic, using divisor enumeration.
        // Early exit is used for the first valid decomposition.
        bool    found  = false;
        int64_t best_p = 0, best_q = 0, best_r = 0, best_s = 0;

        auto    get_divisors = [](int64_t n) -> std::vector<int64_t> {
            std::vector<int64_t> divs;
            n = std::abs(n);
            if (n == 0) {
                divs.push_back(0);
                return divs;
            }
            for (int64_t i = 1; i * i <= n; ++i) {
                if (n % i == 0) {
                    divs.push_back(i);
                    if (i != n / i)
                        divs.push_back(n / i);
                }
            }
            return divs;
        };

        auto divs_a = get_divisors(ia);
        auto divs_c = get_divisors(ic);

        for (int64_t p : divs_a) {
            if (p == 0)
                continue;
            int64_t r = ia / p;
            for (int64_t q : divs_c) {
                int64_t s = (ic == 0) ? 0 : ic / q;
                if (q * s != ic)
                    continue;
                // All sign combinations must be checked to avoid missing
                // negative factors. This is necessary for correctness.
                for (int sp : {1, -1}) {
                    for (int sq : {1, -1}) {
                        for (int sr : {1, -1}) {
                            for (int ss : {1, -1}) {
                                int64_t tp = p * sp;
                                int64_t tq = q * sq;
                                int64_t tr = r * sr;
                                int64_t ts = s * ss;
                                if (tp * tr == ia && tq * ts == ic &&
                                    tp * ts + tq * tr == ib) {
                                    // Normalization: prefer positive leading
                                    // coefficients for output stability.
                                    if (tp < 0) {
                                        tp = -tp;
                                        tq = -tq;
                                    }
                                    if (tr < 0) {
                                        tr = -tr;
                                        ts = -ts;
                                    }
                                    best_p = tp;
                                    best_q = tq;
                                    best_r = tr;
                                    best_s = ts;
                                    found  = true;
                                    goto done;
                                }
                            }
                        }
                    }
                }
            }
        }
    done:

        if (!found)
            return false;

        Polynomial f1(static_cast<double>(best_p), var, 1);
        f1 = f1 + Polynomial(static_cast<double>(best_q));

        Polynomial f2(static_cast<double>(best_r), var, 1);
        f2 = f2 + Polynomial(static_cast<double>(best_s));

        // If both factors are identical, emit as a square for canonicalization.
        if (f1.to_string() == f2.to_string()) {
            factors.push_back({f1, 2});
        } else {
            // Ordering: ensure deterministic output by sorting on leading
            // coefficient and constant term.
            std::string s1 = f1.to_string();
            std::string s2 = f2.to_string();
            if (best_p > best_r || (best_p == best_r && best_q > best_s)) {
                std::swap(f1, f2);
            }
            factors.push_back({f1, 1});
            factors.push_back({f2, 1});
        }

        return true;
    }

    /// Factors a polynomial and returns its `FactoredForm`.
    ///
    /// The algorithm proceeds in order:
    /// 1. Extract the maximal common monomial GCD.
    /// 2. Extract the GCD of all integer-valued coefficients.
    /// 3. Normalize the leading coefficient to be positive.
    /// 4. For univariate degree-2 polynomials, attempt integer factorization
    ///    via `try_factor_quadratic`.
    /// 5. Otherwise return the remaining polynomial as a single irreducible
    ///    factor.
    ///
    /// The resulting `FactoredForm` satisfies:
    ///   `poly ≡ result.numeric_factor * result.common_monomial * ∏ factors`.
    ///
    /// # Arguments
    ///
    /// * `poly` — The polynomial to factor. Must not be modified by the caller
    ///   during the call.
    ///
    /// # Returns
    ///
    /// A `FactoredForm` whose `is_trivial()` is `true` when no factorization
    /// beyond GCD extraction was possible. For the zero polynomial,
    /// `numeric_factor` is 0 and `factors` is empty.
    ///
    /// # Examples
    ///
    /// ```cpp
    /// Polynomial p(1.0, "x", 2);               // x^2
    /// p = p + Polynomial(-1.0, "x", 1);        // x^2 - x
    /// FactoredForm ff = factor_polynomial(p);  // x(x - 1)
    /// // ff.common_monomial == Monomial("x")
    /// // ff.factors == [(x - 1, 1)]
    /// ```
    inline FactoredForm factor_polynomial(const Polynomial& poly) {
        FactoredForm result;

        if (poly.is_zero()) {
            result.numeric_factor = 0;
            return result;
        }

        if (poly.is_constant()) {
            result.numeric_factor = poly.constant_value();
            return result;
        }

        Polynomial working     = poly;

        // Extract maximal monomial GCD. This is required for normalization
        // and to enable further coefficient GCD extraction.
        Monomial   common_mono = working.monomial_gcd();
        if (!common_mono.is_constant()) {
            working                = working.divide_by_monomial(common_mono);
            result.common_monomial = common_mono;
        }

        // Extract GCD of all coefficients. This is necessary to ensure
        // subsequent factoring operates on primitive polynomials.
        double coeff_gcd = working.coefficient_gcd();
        if (coeff_gcd > 1.0 + kCoeffTol) {
            working               = working / coeff_gcd;
            result.numeric_factor = coeff_gcd;
        }

        // Ensure leading coefficient is non-negative for canonicalization.
        if (!working.terms().empty()) {
            auto first_coeff = working.terms().begin()->second;
            if (first_coeff < -kCoeffTol) {
                working = -working;
                result.numeric_factor *= -1;
            }
        }

        if (working.is_constant()) {
            result.numeric_factor *= working.constant_value();
            return result;
        }

        // If only a single term remains, absorb into numeric_factor and
        // common_monomial. This avoids spurious factorization of monomials.
        if (working.num_terms() == 1) {
            auto& [m, c] = *working.terms().begin();
            result.numeric_factor *= c;
            result.common_monomial = result.common_monomial * m;
            return result;
        }

        // Only attempt full factorization for univariate quadratics.
        // This is a deliberate limitation to avoid combinatorial explosion
        // and to keep the factoring logic tractable.
        auto vars = working.variables();
        if (working.is_univariate() && working.degree() == 2) {
            std::string var = working.single_variable();
            std::vector<std::pair<Polynomial, int>> quad_factors;
            if (try_factor_quadratic(working, var, quad_factors)) {
                result.factors = quad_factors;
                return result;
            }
        }

        // Fallback: treat as irreducible for now.
        result.factors.push_back({working, 1});
        return result;
    }

} // namespace math_solver
