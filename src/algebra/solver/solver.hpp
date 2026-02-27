#pragma once

#include "ast/math/equation_expr.hpp"
#include "diagnostics/result.hpp"
#include "runtime/context/context.hpp"
#include <string>

namespace math_solver {

    struct SolveResult {
        std::string variable;
        double      value;
        bool        has_solution;

        // Only returns a string representation if a unique solution exists.
        // Trailing zeros are stripped to match user-facing output conventions.
        // Invariant: If has_solution == false, variable and value are not
        // meaningful.
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

        void                set_input(const std::string& input);

        // Entry point for solving a linear equation with a single unknown.
        //
        // - Assumes upstream validation guarantees linearity and single
        // unknown.
        // - Throws on degenerate or ambiguous cases (multiple unknowns,
        // nonlinear terms).
        // - On success: result.has_solution == true, result.variable names the
        // unique unknown.
        // - Relies on prior syntactic and semantic validation; correctness
        // depends on upstream passes.
        Result<SolveResult> solve(const Equation& eq);

        // Attempts to solve for a specific variable, substituting others from
        // context.
        //
        // - Throws if target variable is absent or eliminated by substitution.
        // - Throws if multiple unknowns remain after substitution.
        // - Designed for context-aware solving; interacts with Context for
        // variable substitution.
        // - On success: result.variable == target_var, all other variables
        // resolved.
        // - Subtle: Care required to avoid accidental elimination of target
        // variable via substitution.
        // - Correctness depends on context consistency and upstream validation.
        Result<SolveResult> solve_for(const Equation&    eq,
                                      const std::string& target_var);
    };

} // namespace math_solver
