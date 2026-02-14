#ifndef SOLVER_H
#define SOLVER_H

#include "../ast/equation.hpp"
#include "../ast/expr.hpp"
#include "../common/error.hpp"
#include "../eval/context.hpp"
#include "linear_collector.hpp"
#include <cmath>
#include <string>
#include <vector>

namespace math_solver {

    // Result of solving an equation
    struct SolveResult {
        std::string variable;     // The variable we solved for
        double      value;        // The solution
        bool        has_solution; // True if exactly one solution exists

        std::string to_string() const {
            if (!has_solution) {
                return "no solution";
            }
            std::string val_str = std::to_string(value);
            // Remove trailing zeros
            size_t      dot_pos = val_str.find('.');
            if (dot_pos != std::string::npos) {
                val_str.erase(val_str.find_last_not_of('0') + 1);
                if (val_str.back() == '.') {
                    val_str.pop_back();
                }
            }
            return variable + " = " + val_str;
        }
    };

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
        // Throws if there are multiple unknowns, non-linear terms, etc.
        SolveResult solve(const Equation& eq) {
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
            result.value        = -b / a;
            result.has_solution = true;

            return result;
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
