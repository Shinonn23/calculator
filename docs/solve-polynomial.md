# Polynomial Equation Solving

## Overview

The `:solve` command automatically detects when an equation is non-linear and polynomial, routing it through a dedicated polynomial solver before falling back to the linear path.

**Supported equation types:**
- **Linear** (degree 1): handled by the exact linear solver (unchanged).
- **Quadratic** (degree 2): exact quadratic formula.
- **Cubic and higher** (degree ≥ 3): Durand–Kerner (Weierstrass) numerical method.

**Multi-root results** are stored as an `ArrayExpr` in the context, enabling broadcast evaluation of subsequent expressions.

## Step-by-step

### 1. Quadratic equation

```
:solve x^2 = 4
  x = [-2, 2] (saved)
```

### 2. Repeated root (double root)

```
:solve x^2 + 2*x + 1 = 0
  x = -1 (multiplicity 2) (saved)
```

### 3. Cubic equation (Durand–Kerner)

```
:solve x^3 - 6*x^2 + 11*x - 6 = 0
  x = [1, 2, 3] (saved)
  (solved via Durand-Kerner)
```

### 4. Broadcast evaluation

After solving an equation with multiple roots, any subsequent expression using the variable is evaluated for each root:

```
:solve x^3 - 6*x^2 + 11*x - 6 = 0
  x = [1, 2, 3] (saved)
x^2
  = [1, 4, 9]
```

### 5. No real solutions

When a polynomial has no real roots, `E0301` is emitted:

```
:solve x^2 + 1 = 0
error[E0301]: no real solutions (discriminant < 0)
```

### 6. Unsupported equations (E0315)

Transcendental expressions (e.g. `sin(x)`) are not polynomial and cannot be solved symbolically. They fall through to the linear solver, which emits `E0315 unsupported`:

```
:solve sin(x) = 0
error[E0315]: non-linear term ...
```

## Checklist

- [x] Equation parsed to `EquationExpr` AST.
- [x] `ASTToPolynomial` converts LHS and RHS to `Polynomial` objects.
- [x] `PolynomialSolver` routes by degree: linear / quadratic / Durand–Kerner.
- [x] Multi-root results stored as `ArrayExpr` in context.
- [x] Broadcast evaluation in `Evaluator::evaluate_broadcast`.
- [x] Complex roots discarded with warning; no-real-solution emits `E0301`.
- [x] Non-polynomial equations fall back to linear solver with `E0315`.

## Error codes

| Code    | Meaning                                 |
| ------- | --------------------------------------- |
| `E0301` | No real solutions                       |
| `E0315` | Unsupported (non-polynomial) expression |
