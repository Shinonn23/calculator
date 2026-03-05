#pragma once

//! # Module — `src/algebra/matrix/solve_method.hpp`
//!
//! Declares the `SolveMethod` enum used by `MatrixSolver` to select between
//! Gaussian elimination and LU decomposition when solving linear systems.

namespace math_solver {
    /// Selects the linear-system solver algorithm used by MatrixSolver.
    enum class SolveMethod {
        Gauss, ///< Gaussian elimination with partial pivoting (default)
        LU,    ///< LU decomposition via Doolittle with partial pivoting
    };
} // namespace math_solver
