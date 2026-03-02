#pragma once

// Central tolerance constants for floating-point comparisons.
//
// Use these instead of hardcoded literals throughout the codebase so that
// thresholds are named, self-documenting, and easy to adjust in one place.
//
// kEpsilon   — "effectively zero" threshold (algebraic precision).
//              Matches the default value of Settings::solver_tolerance.
//              Use for: coefficient zero-tests, equation solving, polynomial
//              pruning, linear-form simplification, LU-decomposition.
//
// kCoeffTol  — "coefficient is ±1 or an integer" threshold (display
//              formatting).  Slightly looser than kEpsilon because display
//              paths only need to distinguish "visually identical to 1" vs
//              "genuinely different".
//              Use for: formatting coefficients in poly/matrix output,
//              rational-number approximation, integer-round checks.
//
// kPivotTol  — "pivot column is numerically singular" threshold used by
//              Gaussian elimination and LU decomposition to detect near-zero
//              pivots before division.  Set between kEpsilon and kCoeffTol.
//              Use for: matrix row-reduction, pivot selection.

namespace math_solver {

    // Mutable globals so Config::load() can override them at runtime.
    // Default values match Settings field defaults; do not change these
    // literals without updating Settings in settings.hpp as well.
    inline double kEpsilon  = 1e-12; // near-zero / algebraic precision
    inline double kCoeffTol = 1e-9;  // coefficient ~1 / near-integer
    inline double kPivotTol = 1e-10; // matrix pivot selection

} // namespace math_solver
