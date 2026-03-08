# Algebra Layer

> **This document has been expanded into detailed sub-pages.**
> Please see **[algebra/README.md](algebra/README.md)** for the full algebra documentation.

## Sub-Pages

| Document                                        | Description                                                  |
| ----------------------------------------------- | ------------------------------------------------------------ |
| [Overview & Index](algebra/README.md)           | Pipeline diagram, navigation, AST class diagram, error codes |
| [Linear Collector](algebra/linear-collector.md) | AST → LinearForm visitor, context substitution               |
| [Simplify](algebra/simplify.md)                 | Canonical form, coefficient formatting, fractions            |
| [Polynomial](algebra/polynomial.md)             | Monomial & Polynomial data structures, arithmetic, GCD       |
| [AST → Polynomial](algebra/ast-to-poly.md)      | ASTToPolynomial visitor, conversion & rejection rules        |
| [Factor](algebra/factor.md)                     | Polynomial factorization pipeline, quadratic factoring       |
| [Equation Solver](algebra/solver.md)            | Single-variable linear solver                                |
| [Polynomial Solver](algebra/poly-solver.md)     | Degree-based dispatch, Durand–Kerner iteration               |
| [Matrix Solver](algebra/matrix-solver.md)       | Systems of equations, Gauss & LU, free variables             |
| [Tolerance](algebra/tolerance.md)               | kEpsilon, kCoeffTol, kPivotTol constants                     |

---

## Further Reading

- [Evaluation](evaluation.md) — How the evaluator calls into the resolver
- [AST](ast.md) — The expression nodes handled by algebra passes
- [Error Codes](../reference/error-codes.md) — Solver-specific error codes (E0300 series)
