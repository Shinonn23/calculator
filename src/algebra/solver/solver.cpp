#include "solver.hpp"
#include "algebra/linear/linear_collector.hpp"
#include "ast/math/equation_expr.hpp"
#include "core/error.hpp"
#include <cmath>
#include <string>
#include <vector>

namespace math_solver {

    void EquationSolver::set_input(const std::string& input) {
        input_ = input;
    }

    SolveResult EquationSolver::solve(const Equation& eq) {
        // Canonicalize equation to lhs - rhs = 0.
        // This normalization is required for downstream passes and ensures
        // variable collection is consistent. Assumes context_ is valid and
        // input_ is canonical for error reporting.
        LinearCollector collector(context_, input_, false);

        LinearForm      lhs        = collector.collect(eq.lhs());
        LinearForm      rhs        = collector.collect(eq.rhs());

        LinearForm      normalized = lhs - rhs;
        normalized.simplify();

        std::set<std::string> unknowns = normalized.variables();

        // Invariant: unknowns.empty() iff equation reduces to a constant.
        // Edge cases:
        //   - constant == 0: tautology, infinite solutions.
        //   - constant != 0: contradiction, no solution.
        if (unknowns.empty()) {
            if (std::abs(normalized.constant) < 1e-12) {
                throw InfiniteSolutionsError(
                    "equation is always true (0 = 0)", eq.span(), input_);
            } else {
                throw NoSolutionError("equation has no solution (" +
                                          std::to_string(normalized.constant) +
                                          " != 0)",
                                      eq.span(),
                                      input_);
            }
        }

        // Only single-unknown linear equations are supported.
        // Multiple unknowns: user error or unsupported input.
        if (unknowns.size() > 1) {
            std::vector<std::string> vars(unknowns.begin(), unknowns.end());
            throw MultipleUnknownsError(vars, eq.span(), input_);
        }

        // At this point, exactly one unknown remains.
        // Invariant: normalized is of the form a*var + b == 0.
        std::string var = *unknowns.begin();
        double      a   = normalized.get_coeff(var);
        double      b   = normalized.constant;

        // Degenerate: variable eliminated by algebraic manipulation.
        // If a == 0, check for tautology or contradiction.
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

        // Unique solution: -b/a.
        // Invariant: result.has_solution is always true on success.
        SolveResult result;
        result.variable     = var;
        result.value        = -b / a;
        result.has_solution = true;

        return result;
    }

    SolveResult EquationSolver::solve_for(const Equation&    eq,
                                          const std::string& target_var) {
        // Defensive: ensure target_var is present in the equation prior to
        // substitutions. Avoids misleading diagnostics if variable was never
        // present or eliminated by prior passes.
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
        // May eliminate variables, including the target, if already defined.
        LinearCollector collector(context_, input_, false);
        LinearForm      lhs        = collector.collect(eq.lhs());
        LinearForm      rhs        = collector.collect(eq.rhs());
        LinearForm      normalized = lhs - rhs;
        normalized.simplify();

        std::set<std::string> unknowns = normalized.variables();

        // If only the target remains, delegate to solve().
        // Ensures consistent error handling and normalization.
        if (unknowns.size() == 1 && unknowns.count(target_var)) {
            return solve(eq);
        }

        // If target_var was fully substituted, solving is not possible.
        // This can occur if the context provides a value for the target.
        if (unknowns.find(target_var) == unknowns.end()) {
            throw InvalidEquationError(
                "variable '" + target_var +
                    "' was substituted from context; cannot solve for it",
                eq.span(),
                input_);
        }

        // If other unknowns remain, equation is underconstrained.
        // Hard error: user must provide more context.
        std::vector<std::string> remaining(unknowns.begin(), unknowns.end());
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

} // namespace math_solver
