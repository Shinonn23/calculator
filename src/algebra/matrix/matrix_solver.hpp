#pragma once

//! Solves systems of linear equations expressed as `LinearForm` objects using
//! Gaussian elimination with partial pivoting or LU decomposition.
//!
//! Pipeline: `LinearForm[]` → augmented matrix `[A|b]` → elimination →
//! `SystemSolveResult`.
//!
//! Invariants:
//! - `forms[i]` represents the equation `forms[i].coeffs * vars =
//! -forms[i].constant`.
//! - `var_order` must list every variable that appears across all forms; extra
//!   variables in forms not in `var_order` are silently ignored.
//! - On success, `result.solutions` maps every variable in `var_order` to its
//! value.

#include "algebra/linear/linear_collector.hpp"
#include "algebra/matrix/solve_method.hpp"
#include "diagnostics/result.hpp"

#include <map>
#include <string>
#include <vector>

namespace math_solver {

    /// Result of solving a linear system.
    struct SystemSolveResult {
        /// Variable-to-value map; populated when a unique solution exists
        /// (`rank(A) == n`).
        std::map<std::string, double> solutions;

        /// Variable-to-parametric-expression map; populated in free-variable
        /// mode when `is_unique` is `false`.
        std::map<std::string, std::string> free_params;

        /// Names of the underdetermined (free) variables when `is_unique` is
        /// `false`.
        std::vector<std::string> free_vars;

        /// `true` iff the system has a unique solution.
        bool   is_unique      = true;

        /// Rank of the coefficient matrix A.
        int    rank_A         = 0;

        /// Rank of the augmented matrix [A|b].
        int    rank_Ab        = 0;

        /// Smallest absolute non-zero pivot encountered during elimination;
        /// used by the `--detect-singular` diagnostic.
        double smallest_pivot = 1e18;
    };

    /// Options that control the solve algorithm.
    struct SolveSystemOptions {
        /// Elimination algorithm; defaults to Gaussian elimination with
        /// partial pivoting.
        SolveMethod method    = SolveMethod::Gauss;

        /// When `true`, parameterise underdetermined systems as free-variable
        /// expressions instead of returning an infinite-solutions error.
        bool        free_vars = false;
    };

    /// Solves a system of linear equations using matrix methods.
    class MatrixSolver {
        /// Raw source text forwarded to error constructors for diagnostics.
        std::string input_;

        public:
        /// Constructs a `MatrixSolver` with a raw source string for diagnostics.
        ///
        /// # Arguments
        ///
        /// * `input` — Original source text; forwarded to `Diagnostic`
        ///   constructors. Defaults to empty string.
        explicit MatrixSolver(const std::string& input = "");

        /// Solves the linear system represented by `forms` for the variables
        /// listed in `var_order`.
        ///
        /// Builds the augmented matrix `[A|b]` where row `i` corresponds to
        /// `forms[i]`: `A[i][j] = forms[i].get_coeff(var_order[j])` and
        /// `b[i] = -forms[i].constant`. Then applies Gaussian elimination or
        /// LU decomposition according to `opts.method`.
        ///
        /// # Arguments
        ///
        /// * `forms`     — One `LinearForm` per equation in the system.
        /// * `var_order` — Ordered list of variable names; determines column
        ///   assignment in the matrix.
        /// * `opts`      — Solver algorithm and free-variable mode selection.
        ///
        /// # Returns
        ///
        /// A `SystemSolveResult` with `is_unique == true` and `solutions`
        /// populated for every variable in `var_order`. When
        /// `opts.free_vars == true` and the system is underdetermined,
        /// `is_unique` is `false` and `free_params` contains parametric
        /// expressions.
        ///
        /// # Errors
        ///
        /// - If `forms` or `var_order` is empty — E0310 (no solution).
        /// - If `rank([A|b]) > rank(A)` — E0310 (inconsistent system).
        /// - If `rank(A) < n` and `opts.free_vars == false` — E0311 (infinite
        ///   solutions).
        Result<SystemSolveResult>
        solve(const std::vector<LinearForm>&  forms,
              const std::vector<std::string>& var_order,
              const SolveSystemOptions&       opts = {});
    };

} // namespace math_solver
