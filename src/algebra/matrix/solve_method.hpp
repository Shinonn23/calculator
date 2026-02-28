#pragma once

namespace math_solver {
    /// Selects the linear-system solver algorithm used by MatrixSolver.
    enum class SolveMethod {
        Gauss, ///< Gaussian elimination with partial pivoting (default)
        LU,    ///< LU decomposition via Doolittle with partial pivoting
    };
} // namespace math_solver
