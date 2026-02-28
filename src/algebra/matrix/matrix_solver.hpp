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

        /// Solve the system represented by `forms` for the variables in
        /// `var_order`.
        ///
        /// Each `LinearForm` encodes the equation
        ///   `sum_j(form.get_coeff(var_order[j]) * var_order[j]) +
        ///   form.constant = 0`
        /// which corresponds to the matrix row `A[i][j] =
        /// form.get_coeff(var_order[j])` and RHS entry `b[i] = -form.constant`.
        ///
        /// Returns an error `Diagnostic` when:
        /// - `rank([A|b]) > rank(A)` — no solution (E0310)
        /// - `rank(A) < n` and `opts.free_vars == false` — infinite solutions
        /// (E0311)
        Result<SystemSolveResult>
        solve(const std::vector<LinearForm>&  forms,
              const std::vector<std::string>& var_order,
              const SolveSystemOptions&       opts = {});
    };

} // namespace math_solver
