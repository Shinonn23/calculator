# Tolerance Constants

> **Source:** `src/core/tolerance.hpp`
>
> Three **mutable global** doubles control numeric precision thresholds throughout the algebra layer.

---

## Constants

| Constant    | Default | Purpose                                                                                                    |
| ----------- | ------- | ---------------------------------------------------------------------------------------------------------- |
| `kEpsilon`  | `1e-12` | Core algebraic precision — near-zero tests in solvers, evaluator, and polynomial cleanup                   |
| `kCoeffTol` | `1e-9`  | Display / integer-detection threshold — decides if a double is "close enough" to an integer for formatting |
| `kPivotTol` | `1e-10` | Matrix pivot threshold — rank determination and singularity detection                                      |

---

## Declarations

```cpp
// src/core/tolerance.hpp
inline double kEpsilon  = 1e-12;
inline double kCoeffTol = 1e-9;
inline double kPivotTol = 1e-10;
```

All three are `inline` mutable globals — modifiable at runtime.

---

## Usage Map

### `kEpsilon` (1e-12) — Algebraic Precision

| Subsystem                 | Usage                                                                                                            |
| ------------------------- | ---------------------------------------------------------------------------------------------------------------- |
| **EquationSolver**        | $\left\lvert a \right\vert < ε$ → coefficient is zero (degeneracy detection)                                     |
| **PolynomialSolver**      | Convergence test: $\left\lvert Δz \right\vert / \left\lvert z \right\vert < ε × 1e-3$ in Durand–Kerner iteration |
| **PolynomialSolver**      | Root classification: $\left\lvert im \right\vert ≤ ε × max(1, \left\lvert re \right\vert)$ → real root           |
| **PolynomialSolver**      | Root deduplication: $\left\lvert distance \right\vert < ε × 10$                                                  |
| **Polynomial::cleanup()** | Remove terms with $\left\lvert lcoeff \right\vert < ε$                                                           |
| **LinearCollector**       | Coefficient near-zero tests                                                                                      |
| **Evaluator**             | Division by zero guard, 0^0 detection                                                                            |
| **GaussianElimination**   | Skip columns with $\left\lvert pivot \right\vert < ε$                                                            |

### `kCoeffTol` (1e-9) — Display / Integer Detection

| Subsystem                   | Usage                                                                                                    |
| --------------------------- | -------------------------------------------------------------------------------------------------------- |
| **Simplifier**              | `format_coefficient()`: detect integer coefficients via $\left\lvert val - round(val) \right\vert < tol$ |
| **Simplifier**              | Fraction mode: `double_to_fraction()` convergence test                                                   |
| **Polynomial::to_string()** | Integer coefficient detection for clean output                                                           |
| **Polynomial GCD**          | `coefficient_gcd()`: integer detection for common factor extraction                                      |
| **MatrixSolver**            | `parameterise_free()`: ±1 detection, near-zero coefficient suppression                                   |

### `kPivotTol` (1e-10) — Matrix Rank

| Subsystem        | Usage                                                                           |
| ---------------- | ------------------------------------------------------------------------------- |
| **MatrixSolver** | `count_rank()`: row is non-zero iff $\left\lvert entry \right\vert > kPivotTol` |
| **MatrixSolver** | LU rank check: diagonal $\left\lvert U[i][i] \right\vert > kPivotTol`           |

---

## Ordering Invariant

The three constants are ordered by strictness:

$$\text{kEpsilon} \;(10^{-12}) \;<\; \text{kPivotTol} \;(10^{-10}) \;<\; \text{kCoeffTol} \;(10^{-9})$$

This means:
- **kEpsilon** is the tightest bound (algebra must be very precise)
- **kPivotTol** is intermediate (matrix rank detection has moderate tolerance)
- **kCoeffTol** is the loosest (display formatting is most forgiving)

---

## Runtime Override

All three constants can be changed at runtime via the `:config set` command in REPL mode:

```
:config set epsilon 1e-15
:config set coeff_tol 1e-12
:config set pivot_tol 1e-13
```

This allows users to tighten or loosen precision for specific problems. Changes persist for the duration of the session.

---

## Design Rationale

**Why mutable globals instead of parameters?**

Tolerance values are read in deeply nested code paths (polynomial cleanup, matrix row operations, root classification). Threading them through every function signature would add significant coupling. Mutable globals provide a simple, practical solution for a single-threaded application.

**Why three separate constants?**

Different subsystems have different precision needs:
- Algebraic operations need the tightest tolerance to avoid cascading errors
- Matrix operations need moderate tolerance since pivoting amplifies error
- Display formatting needs the loosest tolerance since it only affects output presentation, not computation

---

## Further Reading

- [Matrix Solver](matrix-solver.md) — Uses `kEpsilon` and `kPivotTol`
- [Polynomial](polynomial.md) — Uses `kEpsilon` for `cleanup()`
- [Simplify](simplify.md) — Uses `kCoeffTol` for formatting
- [Poly Solver](poly-solver.md) — Uses `kEpsilon` for convergence
