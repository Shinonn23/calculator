#ifndef SOLVER_H
#define SOLVER_H

#include "ast/equation.h"
#include "ast/expr.h"
#include "ast/substitutor.h"
#include "common/error.h"
#include "common/format_utils.h"
#include "domain.h"
#include "eval/context.h"
#include "linear_collector.h"
#include "numerical_solver.h"
#include "quadratic_collector.h"
#include "solve_result.h"
#include <cmath>
#include <string>
#include <vector>

namespace math_solver {

    // Solves linear equations with one unknown variable
    class EquationSolver {
        private:
        const Context* context_;
        std::string    input_;

        public:
        EquationSolver() : context_(nullptr) {}

        explicit EquationSolver(const Context* ctx) : context_(ctx) {}

        EquationSolver(const Context* ctx, const std::string& input)
            : context_(ctx), input_(input) {}

        void        set_input(const std::string& input) { input_ = input; }

        // Solve equation for a single unknown
        // Tries linear first, falls back to quadratic, then numerical
        // Domain constraints are collected and applied to all paths.
        SolveResult solve(const Equation& eq) {
            // Collect domain constraints from the equation
            auto domain = collect_domain(eq.lhs(), eq.rhs());

            try {
                auto result = solve_linear(eq);
                return filter_domain(result, domain, eq);
            } catch (const NonLinearError&) {
                try {
                    auto result = solve_quadratic(eq);
                    return filter_domain(result, domain, eq);
                } catch (const NonLinearError&) {
                    // Fall back to numerical solver — it filters internally
                    return solve_numerical(eq, domain);
                } catch (const MultipleUnknownsError&) {
                    // Re-throw multiple unknowns — numerical can't help
                    throw;
                }
            } catch (const MultipleUnknownsError&) {
                throw;
            }
        }

        // Solve a linear equation (ax + b = 0)
        SolveResult solve_linear(const Equation& eq) {
            // Collect linear form from both sides
            LinearCollector collector(context_, input_, false);

            LinearForm      lhs        = collector.collect(eq.lhs());
            LinearForm      rhs        = collector.collect(eq.rhs());

            // Normalize to: lhs - rhs = 0
            LinearForm      normalized = lhs - rhs;
            normalized.simplify();

            // Get all unknown variables
            std::set<std::string> unknowns = normalized.variables();

            // Check number of unknowns
            if (unknowns.empty()) {
                // Constant equation: c = 0
                if (std::abs(normalized.constant) < 1e-12) {
                    throw InfiniteSolutionsError(
                        "equation is always true (0 = 0)", eq.span(), input_);
                } else {
                    throw NoSolutionError(
                        "equation has no solution (" +
                            std::to_string(normalized.constant) + " != 0)",
                        eq.span(), input_);
                }
            }

            if (unknowns.size() > 1) {
                std::vector<std::string> vars(unknowns.begin(), unknowns.end());
                throw MultipleUnknownsError(vars, eq.span(), input_);
            }

            // Single unknown: ax + b = 0 => x = -b/a
            std::string var = *unknowns.begin();
            double      a   = normalized.get_coeff(var);
            double      b   = normalized.constant;

            if (std::abs(a) < 1e-12) {
                // 0*x + b = 0
                if (std::abs(b) < 1e-12) {
                    throw InfiniteSolutionsError(
                        "equation has infinite solutions (0*" + var + " = 0)",
                        eq.span(), input_);
                } else {
                    throw NoSolutionError("equation has no solution (0*" + var +
                                              " = " + std::to_string(-b) + ")",
                                          eq.span(), input_);
                }
            }

            SolveResult result;
            result.variable     = var;
            result.values       = {-b / a};
            result.has_solution = true;

            return result;
        }

        // Try to solve as quadratic equation (ax^2 + bx + c = 0)
        // Returns empty optional if not quadratic
        SolveResult solve_quadratic(const Equation& eq) {
            QuadraticCollector collector(context_, input_);

            QuadraticForm      lhs        = collector.collect(eq.lhs());
            QuadraticForm      rhs        = collector.collect(eq.rhs());

            QuadraticForm      normalized = lhs - rhs;
            normalized.simplify();

            auto all_vars = normalized.all_variables();

            if (all_vars.empty()) {
                // Constant equation
                if (std::abs(normalized.constant) < 1e-12) {
                    throw InfiniteSolutionsError(
                        "equation is always true (0 = 0)", eq.span(), input_);
                } else {
                    throw NoSolutionError(
                        "equation has no solution (" +
                            std::to_string(normalized.constant) + " != 0)",
                        eq.span(), input_);
                }
            }

            if (all_vars.size() > 1) {
                std::vector<std::string> vars(all_vars.begin(), all_vars.end());
                throw MultipleUnknownsError(vars, eq.span(), input_);
            }

            std::string var = *all_vars.begin();
            double      a   = normalized.get_quad_coeff(var);
            double      b   = normalized.get_linear_coeff(var);
            double      c   = normalized.constant;

            // If no quadratic term, solve linearly
            if (std::abs(a) < 1e-12) {
                if (std::abs(b) < 1e-12) {
                    if (std::abs(c) < 1e-12) {
                        throw InfiniteSolutionsError(
                            "equation has infinite solutions", eq.span(),
                            input_);
                    } else {
                        throw NoSolutionError("equation has no solution",
                                              eq.span(), input_);
                    }
                }
                SolveResult result;
                result.variable     = var;
                result.values       = {-c / b};
                result.has_solution = true;
                return result;
            }

            // Quadratic formula: x = (-b ± √(b²-4ac)) / 2a
            double discriminant = b * b - 4 * a * c;

            if (discriminant < -1e-12) {
                throw NoSolutionError("no real solution (discriminant = " +
                                          format_double(discriminant) + " < 0)",
                                      eq.span(), input_);
            }

            SolveResult result;
            result.variable     = var;
            result.has_solution = true;

            if (std::abs(discriminant) < 1e-12) {
                // One repeated root
                result.values = {-b / (2 * a)};
            } else {
                // Two distinct roots
                double sqrt_d = std::sqrt(discriminant);
                double x1     = (-b - sqrt_d) / (2 * a);
                double x2     = (-b + sqrt_d) / (2 * a);
                // Sort smaller first
                if (x1 > x2)
                    std::swap(x1, x2);
                result.values = {x1, x2};
            }

            return result;
        }

        // Filter an algebraic SolveResult through domain constraints.
        // Removes roots that violate domain and throws if none survive.
        SolveResult filter_domain(SolveResult                          result,
                                  const std::vector<DomainConstraint>& domain,
                                  const Equation&                      eq) {

            if (domain.empty() || !result.has_solution)
                return result;

            std::vector<double>      valid;
            std::vector<std::string> rejected;
            for (double v : result.values) {
                std::string reason =
                    validate_root(domain, result.variable, v, context_, input_);
                if (reason.empty()) {
                    valid.push_back(v);
                } else {
                    rejected.push_back(format_double(v) +
                                       " excluded: " + reason);
                }
            }
            if (valid.empty()) {
                std::string msg = "all roots excluded by domain constraints";
                for (const auto& r : rejected)
                    msg += "\n  " + r;
                throw DomainError(msg, eq.span(), input_);
            }
            result.values       = std::move(valid);
            result.has_solution = true;
            return result;
        }

        // Numerical fallback solver using Newton-Raphson
        SolveResult
        solve_numerical(const Equation&                      eq,
                        const std::vector<DomainConstraint>& domain = {}) {
            // Find the unknown variable by expanding all known context vars
            // and collecting free variables in the expanded equation
            ExprPtr expanded_lhs, expanded_rhs;
            try {
                expanded_lhs =
                    context_ ? context_->expand(eq.lhs()) : eq.lhs().clone();
                expanded_rhs =
                    context_ ? context_->expand(eq.rhs()) : eq.rhs().clone();
            } catch (const std::runtime_error&) {
                expanded_lhs = eq.lhs().clone();
                expanded_rhs = eq.rhs().clone();
            }

            auto                  lhs_vars = free_variables(*expanded_lhs);
            auto                  rhs_vars = free_variables(*expanded_rhs);
            std::set<std::string> all_vars;
            all_vars.insert(lhs_vars.begin(), lhs_vars.end());
            all_vars.insert(rhs_vars.begin(), rhs_vars.end());

            if (all_vars.empty()) {
                // Constant equation — check if true
                Evaluator eval_check(context_, input_);
                double    lv = eval_check.evaluate_scalar(*expanded_lhs);
                double    rv = eval_check.evaluate_scalar(*expanded_rhs);
                if (std::abs(lv - rv) < 1e-12) {
                    throw InfiniteSolutionsError("equation is always true",
                                                 eq.span(), input_);
                } else {
                    throw NoSolutionError("equation has no solution", eq.span(),
                                          input_);
                }
            }

            if (all_vars.size() > 1) {
                std::vector<std::string> vars(all_vars.begin(), all_vars.end());
                throw MultipleUnknownsError(vars, eq.span(), input_);
            }

            std::string     target_var = *all_vars.begin();

            // Use expanded equation for numerical solving
            Equation        expanded_eq(std::move(expanded_lhs),
                                        std::move(expanded_rhs));

            NumericalSolver num_solver(nullptr, input_);
            return num_solver.solve(expanded_eq, target_var, domain);
        }

        // Solve for a specific variable (substitute others from context)
        SolveResult solve_for(const Equation&    eq,
                              const std::string& target_var) {
            // First check if target_var appears in equation
            LinearCollector check_collector(nullptr, input_, true);
            LinearForm      lhs_check = check_collector.collect(eq.lhs());
            LinearForm      rhs_check = check_collector.collect(eq.rhs());
            LinearForm      all_vars  = lhs_check - rhs_check;

            auto            vars      = all_vars.variables();
            if (vars.find(target_var) == vars.end()) {
                throw InvalidEquationError("variable '" + target_var +
                                               "' not found in equation",
                                           eq.span(), input_);
            }

            // Now collect with context (substituting known variables)
            LinearCollector collector(context_, input_, false);
            LinearForm      lhs        = collector.collect(eq.lhs());
            LinearForm      rhs        = collector.collect(eq.rhs());
            LinearForm      normalized = lhs - rhs;
            normalized.simplify();

            std::set<std::string> unknowns = normalized.variables();

            // If target is the only unknown, solve normally
            if (unknowns.size() == 1 && unknowns.count(target_var)) {
                return solve(eq);
            }

            // If target is not among unknowns, it was fully substituted
            if (unknowns.find(target_var) == unknowns.end()) {
                throw InvalidEquationError(
                    "variable '" + target_var +
                        "' was substituted from context; cannot solve for it",
                    eq.span(), input_);
            }

            // Multiple unknowns remain
            std::vector<std::string> remaining(unknowns.begin(),
                                               unknowns.end());
            std::string              hint = "\nHint: use :set to define ";
            for (size_t i = 0; i < remaining.size(); ++i) {
                if (remaining[i] != target_var) {
                    if (i > 0)
                        hint += ", ";
                    hint += remaining[i];
                }
            }
            throw MultipleUnknownsError(remaining, eq.span(), input_);
        }
    };

} // namespace math_solver

#endif
