#ifndef NUMERICAL_SOLVER_H
#define NUMERICAL_SOLVER_H

#include "ast/binary.h"
#include "ast/equation.h"
#include "ast/expr.h"
#include "ast/number.h"
#include "ast/variable.h"
#include "common/error.h"
#include "common/format_utils.h"
#include "domain.h"
#include "eval/context.h"
#include "eval/evaluator.h"
#include "solve_result.h"
#include <algorithm>
#include <cmath>
#include <set>
#include <string>
#include <vector>

namespace math_solver {

    // Numerical equation solver using Newton-Raphson with multiple starting
    // points. Solves f(x) = 0 where f is any expression in a single variable.
    class NumericalSolver {
        private:
        const Context* context_;
        std::string    input_;

        // Evaluate expression with x = val, in a temporary context
        double         eval_at(const Expr& expr, const std::string& var,
                               double val) const {
            Context temp_ctx;
            // Copy all known variables from context
            if (context_) {
                for (const auto& name : context_->all_names()) {
                    const Expr* stored = context_->get_expr(name);
                    if (stored) {
                        temp_ctx.set(name, stored->clone());
                    }
                }
            }
            temp_ctx.set(var, val);

            Evaluator eval(&temp_ctx, input_);
            return eval.evaluate_scalar(expr);
        }

        // Numerical derivative: f'(x) ≈ (f(x+h) - f(x-h)) / (2h)
        double derivative(const Expr& expr, const std::string& var,
                          double x) const {
            double h = std::max(1e-8, std::abs(x) * 1e-8);
            try {
                double fp = eval_at(expr, var, x + h);
                double fm = eval_at(expr, var, x - h);
                return (fp - fm) / (2.0 * h);
            } catch (...) {
                return std::numeric_limits<double>::quiet_NaN();
            }
        }

        // Newton-Raphson from a single starting point
        // Returns NaN if it doesn't converge
        double newton(const Expr& f_expr, const std::string& var, double x0,
                      int max_iter = 200, double tol = 1e-12) const {
            double x = x0;
            for (int i = 0; i < max_iter; ++i) {
                double fx;
                try {
                    fx = eval_at(f_expr, var, x);
                } catch (...) {
                    return std::numeric_limits<double>::quiet_NaN();
                }

                if (std::abs(fx) < tol) {
                    return x; // Converged
                }

                double dfx = derivative(f_expr, var, x);
                if (std::isnan(dfx) || std::abs(dfx) < 1e-15) {
                    // Zero derivative — try a small perturbation
                    x += 0.1;
                    continue;
                }

                double step = fx / dfx;
                // Damping for large steps
                if (std::abs(step) > 100.0) {
                    step = (step > 0 ? 100.0 : -100.0);
                }
                x -= step;

                // Divergence check
                if (std::abs(x) > 1e15) {
                    return std::numeric_limits<double>::quiet_NaN();
                }
            }

            // Check if we're close enough
            try {
                double fx = eval_at(f_expr, var, x);
                if (std::abs(fx) < 1e-8) {
                    return x;
                }
            } catch (...) {
            }

            return std::numeric_limits<double>::quiet_NaN();
        }

        // Check if two values are effectively the same root
        bool is_same_root(double a, double b, double tol = 1e-6) const {
            if (std::abs(a - b) < tol)
                return true;
            // Also check relative tolerance for large values
            double scale = std::max(std::abs(a), std::abs(b));
            if (scale > 1.0 && std::abs(a - b) / scale < tol)
                return true;
            return false;
        }

        // Snap value to nearest integer if very close
        double snap_to_integer(double x, double tol = 1e-9) const {
            double rounded = std::round(x);
            if (std::abs(x - rounded) < tol) {
                return rounded;
            }
            // Also try common fractions: halves, thirds, quarters
            for (int denom = 2; denom <= 8; ++denom) {
                double scaled         = x * denom;
                double rounded_scaled = std::round(scaled);
                if (std::abs(scaled - rounded_scaled) < tol) {
                    return rounded_scaled / denom;
                }
            }
            return x;
        }

        public:
        NumericalSolver() : context_(nullptr) {}

        explicit NumericalSolver(const Context* ctx) : context_(ctx) {}

        NumericalSolver(const Context* ctx, const std::string& input)
            : context_(ctx), input_(input) {}

        // Build the expression (lhs - rhs) so we solve f(x) = 0
        // Then find all real roots using multiple starting points
        SolveResult solve(const Equation& eq, const std::string& var,
                          const std::vector<DomainConstraint>& domain = {}) {
            // Build f(x) = lhs - rhs
            ExprPtr lhs_clone = eq.lhs().clone();
            ExprPtr rhs_clone = eq.rhs().clone();
            auto    f_expr    = std::make_unique<BinaryOp>(
                std::move(lhs_clone), std::move(rhs_clone), BinaryOpType::Sub);

            // Expand variables from context (except the target variable)
            Context solve_ctx;
            if (context_) {
                for (const auto& name : context_->all_names()) {
                    if (name != var) {
                        const Expr* stored = context_->get_expr(name);
                        if (stored) {
                            solve_ctx.set(name, stored->clone());
                        }
                    }
                }
            }

            ExprPtr             expanded_f      = solve_ctx.expand(*f_expr);

            // Try multiple starting points
            std::vector<double> starting_points = {
                0.0,  1.0,  -1.0,  2.0,  -2.0,  3.0,   -3.0,  5.0,
                -5.0, 10.0, -10.0, 0.5,  -0.5,  0.1,   -0.1,  7.0,
                -7.0, 20.0, -20.0, 50.0, -50.0, 100.0, -100.0};

            std::vector<double> roots;
            int                 diverged_count   = 0;
            int                 eval_error_count = 0;
            int                 inconclusive     = 0;

            for (double x0 : starting_points) {
                double root = newton(*expanded_f, var, x0);
                if (std::isnan(root) || !std::isfinite(root)) {
                    diverged_count++;
                    continue;
                }

                // Verify the root
                try {
                    double check = eval_at(*expanded_f, var, root);
                    if (std::abs(check) > 1e-6) {
                        inconclusive++;
                        continue; // Not a valid root
                    }
                } catch (...) {
                    eval_error_count++;
                    continue;
                }

                root        = snap_to_integer(root);

                // Check if this is a new root
                bool is_new = true;
                for (double existing : roots) {
                    if (is_same_root(root, existing)) {
                        is_new = false;
                        break;
                    }
                }
                if (is_new) {
                    roots.push_back(root);
                }
            }

            SolveResult result;
            result.variable     = var;
            result.has_solution = !roots.empty();

            if (roots.empty()) {
                // Provide a differentiated error message
                int total = static_cast<int>(starting_points.size());
                if (eval_error_count == total) {
                    throw NoSolutionError(
                        "equation could not be evaluated at any starting "
                        "point (possible domain issue)",
                        eq.span(), input_);
                }
                if (diverged_count == total) {
                    throw SolverDivergedError(
                        "numerical solver diverged from all " +
                            std::to_string(total) + " starting points",
                        eq.span(), input_);
                }
                std::string detail =
                    "no real solution found (tried " + std::to_string(total) +
                    " starting points: " + std::to_string(diverged_count) +
                    " diverged, " + std::to_string(eval_error_count) +
                    " eval errors, " + std::to_string(inconclusive) +
                    " inconclusive)";
                throw NoSolutionError(detail, eq.span(), input_);
            }

            // Sort roots
            std::sort(roots.begin(), roots.end());

            // Domain-filter: remove roots that violate constraints
            if (!domain.empty()) {
                std::vector<double>      valid;
                std::vector<std::string> rejected_reasons;
                for (double r : roots) {
                    std::string reason =
                        validate_root(domain, var, r, context_, input_);
                    if (reason.empty()) {
                        valid.push_back(r);
                    } else {
                        rejected_reasons.push_back(format_double(r) +
                                                   " excluded: " + reason);
                    }
                }
                if (valid.empty()) {
                    std::string msg =
                        "all roots excluded by domain constraints";
                    for (const auto& rr : rejected_reasons)
                        msg += "\n  " + rr;
                    throw DomainError(msg, eq.span(), input_);
                }
                roots = std::move(valid);
            }

            result.values       = roots;
            result.has_solution = true;
            return result;
        }
    };

} // namespace math_solver

#endif
