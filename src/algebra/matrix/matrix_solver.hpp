#pragma once

//! # Module — `src/algebra/matrix/matrix_solver.hpp`
//!
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
        std::map<std::string, double>
            solutions;   ///< unique solution (rank_A == n)
        std::map<std::string, std::string>
            free_params; ///< var → expression (free-vars mode)
        std::vector<std::string> free_vars;  ///< underdetermined variable names

        bool                     is_unique = true;
        int                      rank_A = 0; ///< rank of coefficient matrix A
        int    rank_Ab                  = 0; ///< rank of augmented matrix [A|b]
        double smallest_pivot =
            1e18; ///< smallest non-zero pivot (for singularity detection)
    };

    /// Options that control the solve algorithm.
    struct SolveSystemOptions {
        SolveMethod method = SolveMethod::Gauss;
        bool        free_vars =
            false; ///< parameterise infinite solutions instead of error
    };

    /// Solves a system of linear equations using matrix methods.
    class MatrixSolver {
        std::string input_; ///< raw source text (for diagnostics)

        public:
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
