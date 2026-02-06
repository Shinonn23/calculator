#ifndef NONLINEAR_SYSTEM_H
#define NONLINEAR_SYSTEM_H

#include "ast/binary.h"
#include "ast/equation.h"
#include "ast/expr.h"
#include "ast/number.h"
#include "ast/substitutor.h"
#include "common/error.h"
#include "common/format_utils.h"
#include "eval/context.h"
#include "eval/evaluator.h"
#include "linear_system.h"
#include <algorithm>
#include <cmath>
#include <memory>
#include <random>
#include <set>
#include <string>
#include <vector>

namespace math_solver {

    // Result of nonlinear system solving
    struct NonlinearSolution {
        SolutionType             type;
        std::vector<std::string> variables;
        std::vector<double>      values;
        std::vector<std::vector<double>>
             all_solutions; // Multiple solution sets

        bool has_solution() const { return type == SolutionType::Unique; }
    };

    class NonlinearSystemSolver {
        private:
        const Context* context_;
        std::string    input_;

        // Evaluate f_i(x) for a system of equations
        double         eval_equation(const Expr& lhs, const Expr& rhs,
                                     const std::vector<std::string>& vars,
                                     const std::vector<double>&      vals) const {
            Context temp_ctx;
            // Copy context variables
            if (context_) {
                for (const auto& name : context_->all_names()) {
                    const Expr* stored = context_->get_expr(name);
                    if (stored)
                        temp_ctx.set(name, stored->clone());
                }
            }
            // Set solving variables
            for (size_t i = 0; i < vars.size(); ++i) {
                temp_ctx.set(vars[i], vals[i]);
            }

            Evaluator eval(&temp_ctx, input_);
            double    lv = eval.evaluate_scalar(lhs);
            double    rv = eval.evaluate_scalar(rhs);
            return lv - rv;
        }

        // Compute numerical Jacobian
        std::vector<std::vector<double>> jacobian(
            const std::vector<std::pair<const Expr*, const Expr*>>& equations,
            const std::vector<std::string>&                         vars,
            const std::vector<double>&                              x) const {

            size_t                           n = vars.size();
            std::vector<std::vector<double>> J(n, std::vector<double>(n, 0.0));

            for (size_t i = 0; i < n; ++i) {
                for (size_t j = 0; j < n; ++j) {
                    double h = std::max(1e-8, std::abs(x[j]) * 1e-8);
                    std::vector<double> xp = x, xm = x;
                    xp[j] += h;
                    xm[j] -= h;

                    try {
                        double fp =
                            eval_equation(*equations[i].first,
                                          *equations[i].second, vars, xp);
                        double fm =
                            eval_equation(*equations[i].first,
                                          *equations[i].second, vars, xm);
                        J[i][j] = (fp - fm) / (2.0 * h);
                    } catch (...) {
                        J[i][j] = 0.0;
                    }
                }
            }
            return J;
        }

        // Solve small linear system Ax = b via Gaussian elimination
        std::vector<double>
        solve_linear_system(std::vector<std::vector<double>> A,
                            std::vector<double>              b) const {

            size_t n = b.size();

            // Forward elimination with partial pivoting
            for (size_t col = 0; col < n; ++col) {
                // Find pivot
                size_t max_row = col;
                double max_val = std::abs(A[col][col]);
                for (size_t row = col + 1; row < n; ++row) {
                    if (std::abs(A[row][col]) > max_val) {
                        max_val = std::abs(A[row][col]);
                        max_row = row;
                    }
                }

                if (max_val < 1e-15) {
                    return {}; // Singular
                }

                // Swap rows
                std::swap(A[col], A[max_row]);
                std::swap(b[col], b[max_row]);

                // Eliminate
                for (size_t row = col + 1; row < n; ++row) {
                    double factor = A[row][col] / A[col][col];
                    for (size_t k = col; k < n; ++k) {
                        A[row][k] -= factor * A[col][k];
                    }
                    b[row] -= factor * b[col];
                }
            }

            // Back substitution
            std::vector<double> x(n);
            for (int i = static_cast<int>(n) - 1; i >= 0; --i) {
                x[i] = b[i];
                for (size_t j = i + 1; j < n; ++j) {
                    x[i] -= A[i][j] * x[j];
                }
                x[i] /= A[i][i];
            }
            return x;
        }

        // Newton-Raphson for systems
        std::vector<double> newton_system(
            const std::vector<std::pair<const Expr*, const Expr*>>& equations,
            const std::vector<std::string>& vars, const std::vector<double>& x0,
            int max_iter = 200, double tol = 1e-10) const {

            size_t              n = vars.size();
            std::vector<double> x = x0;

            for (int iter = 0; iter < max_iter; ++iter) {
                // Evaluate residuals f(x)
                std::vector<double> f(n);
                bool                all_ok = true;
                for (size_t i = 0; i < n; ++i) {
                    try {
                        f[i] = eval_equation(*equations[i].first,
                                             *equations[i].second, vars, x);
                    } catch (...) {
                        all_ok = false;
                        break;
                    }
                }
                if (!all_ok)
                    return {};

                // Check convergence
                double max_residual = 0.0;
                for (double fi : f) {
                    max_residual = std::max(max_residual, std::abs(fi));
                }
                if (max_residual < tol) {
                    return x; // Converged
                }

                // Compute Jacobian
                auto J     = jacobian(equations, vars, x);

                // Solve J * delta = f
                auto delta = solve_linear_system(J, f);
                if (delta.empty())
                    return {}; // Singular Jacobian

                // Update x with damping
                double max_step = 0.0;
                for (double d : delta) {
                    max_step = std::max(max_step, std::abs(d));
                }

                double damping = 1.0;
                if (max_step > 50.0) {
                    damping = 50.0 / max_step;
                }

                for (size_t i = 0; i < n; ++i) {
                    x[i] -= damping * delta[i];
                }

                // Divergence check
                bool diverged = false;
                for (double xi : x) {
                    if (std::abs(xi) > 1e12 || std::isnan(xi)) {
                        diverged = true;
                        break;
                    }
                }
                if (diverged)
                    return {};
            }

            return {}; // Didn't converge
        }

        // Snap value to nearest integer/simple fraction
        double snap(double x, double tol = 1e-8) const {
            double rounded = std::round(x);
            if (std::abs(x - rounded) < tol)
                return rounded;
            for (int d = 2; d <= 8; ++d) {
                double scaled = x * d;
                double rs     = std::round(scaled);
                if (std::abs(scaled - rs) < tol)
                    return rs / d;
            }
            return x;
        }

        bool is_same_solution(const std::vector<double>& a,
                              const std::vector<double>& b,
                              double                     tol = 1e-6) const {
            if (a.size() != b.size())
                return false;
            for (size_t i = 0; i < a.size(); ++i) {
                if (std::abs(a[i] - b[i]) > tol)
                    return false;
            }
            return true;
        }

        public:
        NonlinearSystemSolver() : context_(nullptr) {}

        explicit NonlinearSystemSolver(const Context* ctx) : context_(ctx) {}

        NonlinearSystemSolver(const Context* ctx, const std::string& input)
            : context_(ctx), input_(input) {}

        NonlinearSolution
        solve(const std::vector<std::unique_ptr<Equation>>& equations,
              const std::vector<std::string>&               vars) {

            size_t                                           n = vars.size();

            // Build equation pointer pairs
            std::vector<std::pair<const Expr*, const Expr*>> eq_ptrs;
            for (const auto& eq : equations) {
                eq_ptrs.push_back({&eq->lhs(), &eq->rhs()});
            }

            // Try many starting points
            std::vector<std::vector<double>> all_solutions;
            std::mt19937                     rng(42); // Deterministic seed
            std::uniform_real_distribution<double> dist(-10.0, 10.0);

            // Structured starting points
            std::vector<std::vector<double>>       starts;
            // Grid of common values
            std::vector<double> grid = {0, 1, -1, 2, -2, 3, -3, 4, -4, 5, -5};
            for (double a : grid) {
                for (double b : grid) {
                    if (n == 2) {
                        starts.push_back({a, b});
                    }
                }
            }
            // Also random starting points
            for (int r = 0; r < 50; ++r) {
                std::vector<double> pt(n);
                for (size_t i = 0; i < n; ++i) {
                    pt[i] = dist(rng);
                }
                starts.push_back(pt);
            }

            for (const auto& x0 : starts) {
                auto result = newton_system(eq_ptrs, vars, x0);
                if (result.empty())
                    continue;

                // Verify solution
                bool valid = true;
                for (size_t i = 0; i < n; ++i) {
                    try {
                        double residual =
                            eval_equation(*eq_ptrs[i].first, *eq_ptrs[i].second,
                                          vars, result);
                        if (std::abs(residual) > 1e-6) {
                            valid = false;
                            break;
                        }
                    } catch (...) {
                        valid = false;
                        break;
                    }
                }
                if (!valid)
                    continue;

                // Snap values
                for (double& v : result) {
                    v = snap(v);
                }

                // Check if duplicate
                bool is_new = true;
                for (const auto& existing : all_solutions) {
                    if (is_same_solution(result, existing)) {
                        is_new = false;
                        break;
                    }
                }
                if (is_new) {
                    all_solutions.push_back(result);
                }
            }

            NonlinearSolution sol;
            sol.variables = vars;

            if (all_solutions.empty()) {
                sol.type = SolutionType::NoSolution;
            } else {
                sol.type          = SolutionType::Unique;
                sol.values        = all_solutions[0]; // Primary solution
                sol.all_solutions = all_solutions;
            }

            return sol;
        }
    };

} // namespace math_solver

#endif
