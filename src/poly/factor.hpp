#ifndef FACTOR_H
#define FACTOR_H

#include "../common/color.hpp"
#include "../common/error.hpp"
#include "../parser/parser.hpp"
#include "ast_to_poly.hpp"
#include "polynomial.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace math_solver {

    // ========================================================================
    // FactoredForm — represents a polynomial as a product of factors
    //   numeric_factor * product_of( (factor_poly, exponent) )
    // ========================================================================
    struct FactoredForm {
        double                                  numeric_factor = 1.0;
        Monomial                                common_monomial;
        std::vector<std::pair<Polynomial, int>> factors; // (poly, exp)

        // Check if nothing was actually factored (irreducible)
        bool is_trivial() const {
            return std::abs(numeric_factor - 1.0) < 1e-9 &&
                   common_monomial.is_constant() && factors.size() == 1 &&
                   factors[0].second == 1;
        }

        std::string to_string() const {
            // If nothing was factored, return plain polynomial string
            if (is_trivial()) {
                return factors[0].first.to_string();
            }

            std::string result;

            // Numeric coefficient
            bool has_numeric = std::abs(numeric_factor - 1.0) > 1e-9 &&
                               std::abs(numeric_factor + 1.0) > 1e-9;
            bool is_negative = numeric_factor < 0;

            // Handle -1 case with no other factors
            if (std::abs(numeric_factor + 1.0) < 1e-9 &&
                common_monomial.is_constant() && factors.empty()) {
                return "-1";
            }

            if (has_numeric) {
                double abs_val = std::abs(numeric_factor);
                if (is_negative)
                    result += "-";
                if (std::abs(abs_val - std::round(abs_val)) < 1e-9) {
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

            // Common monomial (e.g. xy in xy(x - 3))
            std::string mono_str = common_monomial.to_string();
            if (!mono_str.empty()) {
                result += mono_str;
            }

            // Factors
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

    // ========================================================================
    // Factoring engine
    // ========================================================================

    // Try to factor a quadratic ax^2 + bx + c over integers
    // Returns true if factored, fills factors
    inline bool
    try_factor_quadratic(const Polynomial& poly, const std::string& var,
                         std::vector<std::pair<Polynomial, int>>& factors) {

        double a = poly.coeff_of_degree(2);
        double b = poly.coeff_of_degree(1);
        double c = poly.coeff_of_degree(0);

        // Check all coefficients are integers
        if (std::abs(a - std::round(a)) > 1e-9 ||
            std::abs(b - std::round(b)) > 1e-9 ||
            std::abs(c - std::round(c)) > 1e-9) {
            return false;
        }

        int64_t ia = static_cast<int64_t>(std::round(a));
        int64_t ib = static_cast<int64_t>(std::round(b));
        int64_t ic = static_cast<int64_t>(std::round(c));

        if (ia == 0)
            return false;

        // Discriminant
        int64_t disc = ib * ib - 4 * ia * ic;
        if (disc < 0)
            return false;

        // Check if discriminant is a perfect square
        int64_t sqrt_disc = static_cast<int64_t>(
            std::round(std::sqrt(static_cast<double>(std::abs(disc)))));
        if (sqrt_disc * sqrt_disc != disc)
            return false;

        // Roots: (-b ± sqrt(disc)) / (2a)
        // We need integer or rational roots that produce integer factors
        // For ax^2 + bx + c, we find p, q, r, s such that
        //   ax^2 + bx + c = a/a * (px + q)(rx + s)
        //   where p*r = a, q*s = c, p*s + q*r = b

        // Try all factor pairs of a and c
        bool    found  = false;
        int64_t best_p = 0, best_q = 0, best_r = 0, best_s = 0;

        // Generate divisors of |a| and |c|
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
                // Try all sign combinations
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
                                    // Normalize: make leading coefficients
                                    // positive
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

        // Build factor polynomials: (px + q) and (rx + s)
        Polynomial f1(static_cast<double>(best_p), var, 1);
        f1 = f1 + Polynomial(static_cast<double>(best_q));

        Polynomial f2(static_cast<double>(best_r), var, 1);
        f2 = f2 + Polynomial(static_cast<double>(best_s));

        // Check if f1 == f2 (perfect square)
        if (f1.to_string() == f2.to_string()) {
            factors.push_back({f1, 2});
        } else {
            // Order: factor with smaller leading coeff first, then smaller
            // constant
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

    // Main factoring function
    inline FactoredForm factor_polynomial(const Polynomial& poly) {
        FactoredForm result;

        if (poly.is_zero()) {
            result.numeric_factor = 0;
            return result;
        }

        // If it's a constant, just return it
        if (poly.is_constant()) {
            result.numeric_factor = poly.constant_value();
            return result;
        }

        Polynomial working     = poly;

        // Step 1: Extract common monomial factor
        Monomial   common_mono = working.monomial_gcd();
        if (!common_mono.is_constant()) {
            working                = working.divide_by_monomial(common_mono);
            result.common_monomial = common_mono;
        }

        // Step 2: Extract GCD of coefficients
        double coeff_gcd = working.coefficient_gcd();
        if (coeff_gcd > 1.0 + 1e-9) {
            working               = working / coeff_gcd;
            result.numeric_factor = coeff_gcd;
        }

        // Check sign: if leading coefficient is negative, factor out -1
        if (!working.terms().empty()) {
            auto first_coeff = working.terms().begin()->second;
            if (first_coeff < -1e-9) {
                working = -working;
                result.numeric_factor *= -1;
            }
        }

        // If after extraction we have a constant, done
        if (working.is_constant()) {
            result.numeric_factor *= working.constant_value();
            return result;
        }

        // If single term, no further factoring needed —
        // but the monomial part should be in common_monomial
        if (working.num_terms() == 1) {
            auto& [m, c] = *working.terms().begin();
            result.numeric_factor *= c;
            // Merge the monomial into common_monomial
            result.common_monomial = result.common_monomial * m;
            return result;
        }

        // Step 3: Try quadratic factorization (univariate degree 2)
        auto vars = working.variables();
        if (working.is_univariate() && working.degree() == 2) {
            std::string var = working.single_variable();
            std::vector<std::pair<Polynomial, int>> quad_factors;
            if (try_factor_quadratic(working, var, quad_factors)) {
                result.factors = quad_factors;
                return result;
            }
        }

        // Step 4: No further factoring — return as-is
        result.factors.push_back({working, 1});
        return result;
    }

    inline void cmd_factor(const std::string& args) {
        if (args.empty()) {
            std::cout << "  Usage: factor <polynomial>\n";
            return;
        }

        try {
            Parser          parser(args);
            auto            expr = parser.parse();

            ASTToPolynomial converter(args);
            Polynomial      poly     = converter.convert(*expr);

            FactoredForm    factored = factor_polynomial(poly);
            std::string     output   = factored.to_string();

            std::cout << "  " << output << "\n";
        } catch (const MathError& e) {
            std::cout << e.format() << "\n";
        } catch (const std::exception& e) {
            std::cout << ansi::red << "  Error: " << ansi::reset << e.what()
                      << "\n";
        }
    }

} // namespace math_solver

#endif
