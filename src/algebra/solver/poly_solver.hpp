#pragma once

//! # Module — `src/algebra/solver/poly_solver.hpp`
//!
//! Solves univariate polynomial equations of the form P(x) = 0 where P is
//! produced by subtracting the RHS polynomial from the LHS.
//!
//! Strategy by degree:
//!   - Degree 0 : constant — no variable, handled as no-solution or tautology.
//!   - Degree 1 : exact linear solve  (-b / a).
//!   - Degree 2 : exact quadratic formula (real roots only).
//!   - Degree 3+: Durand–Kerner (Weierstrass) numerical method.
//!
//! All roots are returned as `PolyRoots` containing:
//!   - `real_roots`     — sorted ascending, imaginary part filtered by `tol`.
//!   - `all_roots`      — all complex roots (may include conjugate pairs).
//!   - `multiplicities` — multiplicity count parallel to `real_roots`.

#include "algebra/polynomial/polynomial.hpp"
#include "diagnostics/kinds/solver_errors.hpp"
#include "diagnostics/result.hpp"

#include <algorithm>
#include <cmath>
#include <complex>
#include <string>
#include <vector>

namespace math_solver {

    /// Result type for polynomial root-finding.
    struct PolyRoots {
        /// Real roots sorted ascending. Length == multiplicities.size().
        std::vector<double> real_roots;

        /// All complex roots returned by the solver (including real ones).
        std::vector<std::complex<double>> all_roots;

        /// Multiplicities parallel to real_roots.
        /// A value > 1 indicates a repeated root.
        std::vector<int> multiplicities;

        /// Number of complex-only (non-real) roots discarded.
        int complex_count = 0;

        bool has_real() const { return !real_roots.empty(); }
    };

    /// Solves univariate polynomial equations.
    ///
    /// Thread-safety: stateless — safe to use from multiple threads.
    class PolynomialSolver {
        public:
        /// Solve poly = 0 where poly is univariate.
        ///
        /// @param poly   The polynomial (lhs − rhs already normalised to = 0).
        /// @param input  Source string for error messages.
        /// @param tol    Tolerance for Im-part filtering and root-merging.
        Result<PolyRoots> solve(const Polynomial& poly,
                                const std::string& input = "",
                                double             tol   = 1e-9) const {
            if (!poly.is_univariate()) {
                return Result<PolyRoots>::err(errors::invalid_equation(
                    "polynomial solver requires a univariate polynomial",
                    Span{}, input));
            }

            int deg = poly.degree();

            // Degree 0: constant polynomial.
            if (deg == 0) {
                double c = poly.constant_value();
                if (std::abs(c) < tol) {
                    return Result<PolyRoots>::err(errors::infinite_solutions(
                        "equation is always true (0 = 0)", Span{}, input));
                }
                return Result<PolyRoots>::err(errors::no_solution(
                    "no real solutions", Span{}, input));
            }

            // Extract coefficients [a_n, a_{n-1}, ..., a_1, a_0]
            // where the polynomial is a_n*x^n + ... + a_0.
            std::string              var   = poly.single_variable();
            std::vector<double>      coeffs = extract_coeffs(poly, var, deg);

            // Degree 1: exact.
            if (deg == 1) {
                double a = coeffs[0]; // leading (x^1)
                double b = coeffs[1]; // constant
                if (std::abs(a) < tol) {
                    if (std::abs(b) < tol)
                        return Result<PolyRoots>::err(errors::infinite_solutions(
                            "equation is always true", Span{}, input));
                    return Result<PolyRoots>::err(errors::no_solution(
                        "no real solutions", Span{}, input));
                }
                PolyRoots r;
                double    root = -b / a;
                r.real_roots.push_back(root);
                r.all_roots.emplace_back(root, 0.0);
                r.multiplicities.push_back(1);
                return Result<PolyRoots>::ok(r);
            }

            // Degree 2: quadratic formula.
            if (deg == 2) {
                return solve_quadratic(coeffs, tol, input);
            }

            // Degree 3+: Durand–Kerner numerical method.
            return solve_numerical(coeffs, deg, tol, input);
        }

        private:
        // ── Helpers ──────────────────────────────────────────────────────────

        // Extract coefficients from highest to lowest degree.
        // Returns vector of size (deg + 1): coeffs[0] = a_n, coeffs[deg] = a_0.
        std::vector<double> extract_coeffs(const Polynomial& poly,
                                           const std::string& var,
                                           int                deg) const {
            std::vector<double> c(static_cast<size_t>(deg + 1), 0.0);
            for (int k = 0; k <= deg; ++k) {
                // coeff of x^k
                double v;
                if (k == 0) {
                    v = poly.constant_value();
                } else {
                    Monomial m(var, k);
                    v = poly.coefficient(m);
                }
                c[static_cast<size_t>(deg - k)] = v;
            }
            return c;
        }

        // Evaluate polynomial (Horner's scheme).
        // coeffs[0] = leading coeff, coeffs[n] = constant term.
        std::complex<double>
        eval_poly_c(const std::vector<double>&  c,
                    std::complex<double>         x) const {
            std::complex<double> result = c[0];
            for (size_t i = 1; i < c.size(); ++i)
                result = result * x + c[i];
            return result;
        }

        // Quadratic formula for degree-2 case.
        // coeffs = [a, b, c] for ax^2 + bx + c = 0.
        Result<PolyRoots> solve_quadratic(const std::vector<double>& coeffs,
                                          double             tol,
                                          const std::string& input) const {
            double a = coeffs[0], b = coeffs[1], c = coeffs[2];
            double disc = b * b - 4.0 * a * c;

            PolyRoots r;

            if (disc < -tol) {
                // Two complex roots — no real roots.
                double re   = -b / (2.0 * a);
                double im   = std::sqrt(-disc) / (2.0 * a);
                r.all_roots.emplace_back(re, im);
                r.all_roots.emplace_back(re, -im);
                r.complex_count = 2;
                return Result<PolyRoots>::err(errors::no_solution(
                    "no real solutions (discriminant < 0)", Span{}, input));
            }

            if (std::abs(disc) <= tol) {
                // One repeated root.
                double root = -b / (2.0 * a);
                r.real_roots.push_back(root);
                r.all_roots.emplace_back(root, 0.0);
                r.multiplicities.push_back(2);
            } else {
                // Two distinct real roots.
                double sq   = std::sqrt(disc);
                double r1   = (-b - sq) / (2.0 * a);
                double r2   = (-b + sq) / (2.0 * a);
                if (r1 > r2)
                    std::swap(r1, r2);
                r.real_roots = {r1, r2};
                r.all_roots  = {std::complex<double>(r1, 0.0),
                                std::complex<double>(r2, 0.0)};
                r.multiplicities = {1, 1};
            }

            return Result<PolyRoots>::ok(r);
        }

        // Durand–Kerner (Weierstrass) root-finding for degree >= 3.
        // coeffs[0] = leading, coeffs[deg] = constant.
        Result<PolyRoots>
        solve_numerical(const std::vector<double>& coeffs, int deg,
                        double             tol,
                        const std::string& input) const {
            // Normalize to monic polynomial.
            double              lead = coeffs[0];
            std::vector<double> monic(coeffs.size());
            for (size_t i = 0; i < monic.size(); ++i)
                monic[i] = coeffs[i] / lead;

            // Initial approximations: spread on a circle of radius r in the
            // complex plane, where r is chosen to bound roots via Cauchy's
            // bound.
            double r_bound = 1.0;
            for (size_t i = 1; i < monic.size(); ++i)
                r_bound = std::max(r_bound, std::abs(monic[i]));
            r_bound = 1.0 + r_bound;

            const double             PI = std::acos(-1.0);
            std::vector<std::complex<double>> roots(static_cast<size_t>(deg));
            for (int k = 0; k < deg; ++k) {
                double angle = 2.0 * PI * k / deg + PI / (2.0 * deg);
                roots[static_cast<size_t>(k)] =
                    r_bound * std::complex<double>(std::cos(angle),
                                                   std::sin(angle));
            }

            // Iterate Durand–Kerner.
            const int max_iter = 2000;
            for (int iter = 0; iter < max_iter; ++iter) {
                double max_delta = 0.0;
                for (int i = 0; i < deg; ++i) {
                    size_t               ii  = static_cast<size_t>(i);
                    std::complex<double> num = eval_poly_c(monic, roots[ii]);
                    std::complex<double> den(1.0, 0.0);
                    for (int j = 0; j < deg; ++j) {
                        if (j != i)
                            den *= (roots[ii] -
                                    roots[static_cast<size_t>(j)]);
                    }
                    // Guard against near-zero denominator.
                    if (std::abs(den) < 1e-300)
                        continue;
                    std::complex<double> delta = num / den;
                    roots[ii] -= delta;
                    max_delta  = std::max(max_delta, std::abs(delta));
                }
                if (max_delta < tol * 1e-3)
                    break;
            }

            // Polish roots: a few Newton steps.
            for (int i = 0; i < deg; ++i) {
                size_t ii = static_cast<size_t>(i);
                for (int step = 0; step < 8; ++step) {
                    std::complex<double> fx  = eval_poly_c(monic, roots[ii]);
                    // Derivative via Horner.
                    std::complex<double> dfx = monic[0];
                    for (size_t k = 1; k + 1 < monic.size(); ++k)
                        dfx = dfx * roots[ii] + monic[k];
                    if (std::abs(dfx) < 1e-300)
                        break;
                    roots[ii] -= fx / dfx;
                }
            }

            // Classify roots.
            PolyRoots result;
            result.all_roots = roots;

            std::vector<double> real_cands;
            for (const auto& rt : roots) {
                if (std::abs(rt.imag()) <= tol * std::max(1.0, std::abs(rt.real())))
                    real_cands.push_back(rt.real());
                else
                    ++result.complex_count;
            }

            // Sort and merge near-duplicate real roots.
            std::sort(real_cands.begin(), real_cands.end());
            for (size_t i = 0; i < real_cands.size(); ) {
                double val  = real_cands[i];
                int    mult = 1;
                size_t j    = i + 1;
                while (j < real_cands.size() &&
                       std::abs(real_cands[j] - val) <=
                           tol * std::max(1.0, std::abs(val))) {
                    ++mult;
                    ++j;
                }
                result.real_roots.push_back(val);
                result.multiplicities.push_back(mult);
                i = j;
            }

            if (result.real_roots.empty()) {
                return Result<PolyRoots>::err(errors::no_solution(
                    "no real solutions", Span{}, input));
            }

            return Result<PolyRoots>::ok(result);
        }
    };

} // namespace math_solver
