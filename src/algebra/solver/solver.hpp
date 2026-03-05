#pragma once

//! # Module — `src/algebra/solver/solver.hpp`
//!
//! Defines `SolveResult` and `EquationSolver` — the single-equation linear
//! solver. `EquationSolver` accepts an `Equation` AST node, collects its
//! `LinearForm` via `LinearCollector`, and solves for the unique unknown using
//! exact arithmetic (`−b / a`).
//!
//! For polynomial (non-linear) equations, callers should use
//! `PolynomialSolver` instead. For multi-variable systems, use `MatrixSolver`.

#include "ast/math/equation_expr.hpp"
#include "diagnostics/result.hpp"
#include "runtime/context/context.hpp"
#include <string>

namespace math_solver {

    /// Holds the result of solving a single-variable linear equation.
    ///
    /// When `has_solution` is `false`, `variable` and `value` are
    /// meaningless — inspect the `Diagnostic` returned by `EquationSolver`
    /// instead.
    struct SolveResult {
        /// Name of the solved variable.
        std::string variable;

        /// Numeric value of the solution.
        double      value;

        /// `true` iff a unique solution was found.
        bool        has_solution;

        /// Returns `"variable = value"` with trailing zeros stripped, or
        /// `"no solution"` when `has_solution` is `false`.
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

    /// Solves a single-variable linear equation for its unique unknown.
    ///
    /// Uses `LinearCollector` to reduce the equation to the form `a·x + b = 0`,
    /// then returns `x = −b / a`. Context bindings are substituted unless the
    /// caller passes a null context.
    class EquationSolver {
        private:
        const Context* context_;
        std::string    input_;

        public:
        /// Constructs a solver with no context.
        EquationSolver() : context_(nullptr) {}

        /// Constructs a solver backed by `ctx` for variable substitution.
        explicit EquationSolver(const Context* ctx) : context_(ctx) {}

        /// Constructs a solver backed by `ctx` with a raw source string for
        /// diagnostics.
        EquationSolver(const Context* ctx, const std::string& input)
            : context_(ctx), input_(input) {}

        /// Sets the raw source string used in diagnostic messages.
        void                set_input(const std::string& input);

        /// Solves `eq` for its single unknown variable.
        ///
        /// Normalizes to `LHS − RHS = 0`, collects variables, and returns
        /// `x = −b / a`.
        ///
        /// # Arguments
        ///
        /// * `eq` — The equation to solve; must be linear in exactly one
        ///   unknown after context substitution.
        ///
        /// # Returns
        ///
        /// A `SolveResult` with `has_solution == true` and the solved value.
        ///
        /// # Errors
        ///
        /// - If the equation reduces to `0 = 0` — infinite solutions (E0312).
        /// - If the equation reduces to `c = 0` (c ≠ 0) — no solution (E0313).
        /// - If more than one unknown remains after substitution — multiple
        ///   unknowns error.
        /// - If the linear coefficient is zero after collection — no solution
        ///   or infinite solutions depending on the constant.
        Result<SolveResult> solve(const Equation& eq);

        /// Solves `eq` for the specific variable `target_var`.
        ///
        /// Verifies that `target_var` is present in the equation before
        /// performing context substitution. After substitution, delegates to
        /// `solve` if only the target remains; errors if the target was
        /// eliminated by the context or if other unknowns remain.
        ///
        /// # Arguments
        ///
        /// * `eq`         — The equation to solve.
        /// * `target_var` — The variable to solve for; must appear in `eq`.
        ///
        /// # Returns
        ///
        /// A `SolveResult` with `result.variable == target_var`.
        ///
        /// # Errors
        ///
        /// - If `target_var` does not appear in `eq` — invalid equation error.
        /// - If `target_var` was substituted away by the context — invalid
        ///   equation error.
        /// - If other unknowns remain after substitution — multiple unknowns
        ///   error.
        Result<SolveResult> solve_for(const Equation&    eq,
                                      const std::string& target_var);
    };

} // namespace math_solver
