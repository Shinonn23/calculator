#pragma once

#include "algebra/linear/linear_collector.hpp"
#include "ast/math/equation_expr.hpp"
#include "core/error.hpp"
#include "runtime/context/context.hpp"
#include <cmath>
#include <string>
#include <vector>

namespace math_solver {

    struct SolveResult {
        std::string variable;
        double      value;
        bool        has_solution;

        // Only returns a string representation if a unique solution exists.
        // Trailing zeros are stripped for consistency with user-facing output.
        std::string to_string() const {
            if (!has_solution) {
                return "no solution";
            }
            std::string val_str = std::to_string(value);
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

        // Entry point for solving a linear equation with a single unknown.
        // - Assumes input is already validated as a linear equation.
        // - Throws on degenerate cases (multiple unknowns, nonlinear terms,
        // etc).
        // - Invariant: If returned, result.has_solution == true and
        // result.variable is the unique unknown.
        SolveResult solve(const Equation& eq) {
            LinearCollector collector(context_, input_, false);

            LinearForm      lhs        = collector.collect(eq.lhs());
            LinearForm      rhs        = collector.collect(eq.rhs());

            // Always normalize to canonical form: lhs - rhs = 0.
            LinearForm      normalized = lhs - rhs;
            normalized.simplify();

            std::set<std::string> unknowns = normalized.variables();

            // Handle degenerate cases up front to avoid silent miscompilation:
            // - No unknowns: equation is either tautological or unsatisfiable.
            if (unknowns.empty()) {
                if (std::abs(normalized.constant) < 1e-12) {
                    throw InfiniteSolutionsError(
                        "equation is always true (0 = 0)", eq.span(), input_);
                } else {
                    throw NoSolutionError(
                        "equation has no solution (" +
                            std::to_string(normalized.constant) + " != 0)",
                        eq.span(),
                        input_);
                }
            }

            // Multiple unknowns are not supported by this solver.
            if (unknowns.size() > 1) {
                std::vector<std::string> vars(unknowns.begin(), unknowns.end());
                throw MultipleUnknownsError(vars, eq.span(), input_);
            }

            // At this point, exactly one unknown remains.
            std::string var = *unknowns.begin();
            double      a   = normalized.get_coeff(var);
            double      b   = normalized.constant;

            // If coefficient of unknown vanishes, check for infinite or no
            // solutions.
            if (std::abs(a) < 1e-12) {
                if (std::abs(b) < 1e-12) {
                    throw InfiniteSolutionsError(
                        "equation has infinite solutions (0*" + var + " = 0)",
                        eq.span(),
                        input_);
                } else {
                    throw NoSolutionError("equation has no solution (0*" + var +
                                              " = " + std::to_string(-b) + ")",
                                          eq.span(),
                                          input_);
                }
            }

            SolveResult result;
            result.variable     = var;
            result.value        = -b / a;
            result.has_solution = true;

            return result;
        }

        // Attempts to solve for a specific variable, substituting others from
        // context.
        // - Throws if the target variable is not present or is eliminated by
        // substitution.
        // - Throws if multiple unknowns remain after substitution.
        // - This is the main entry point for context-aware solving.
        SolveResult solve_for(const Equation&    eq,
                              const std::string& target_var) {
            // Defensive: ensure target_var is present in the equation before
            // substitution.
            LinearCollector check_collector(nullptr, input_, true);
            LinearForm      lhs_check = check_collector.collect(eq.lhs());
            LinearForm      rhs_check = check_collector.collect(eq.rhs());
            LinearForm      all_vars  = lhs_check - rhs_check;

            auto            vars      = all_vars.variables();
            if (vars.find(target_var) == vars.end()) {
                throw InvalidEquationError("variable '" + target_var +
                                               "' not found in equation",
                                           eq.span(),
                                           input_);
            }

            // Substitute known variables from context.
            LinearCollector collector(context_, input_, false);
            LinearForm      lhs        = collector.collect(eq.lhs());
            LinearForm      rhs        = collector.collect(eq.rhs());
            LinearForm      normalized = lhs - rhs;
            normalized.simplify();

            std::set<std::string> unknowns = normalized.variables();

            // If only the target remains, delegate to solve().
            if (unknowns.size() == 1 && unknowns.count(target_var)) {
                return solve(eq);
            }

            // If target_var was fully substituted, solving is not possible.
            if (unknowns.find(target_var) == unknowns.end()) {
                throw InvalidEquationError(
                    "variable '" + target_var +
                        "' was substituted from context; cannot solve for it",
                    eq.span(),
                    input_);
            }

            // If other unknowns remain, solving is ambiguous.
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
