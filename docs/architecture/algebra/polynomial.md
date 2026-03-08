# Polynomial

> **Source:** `src/algebra/polynomial/polynomial.hpp`
>
> Core algebraic data structures — `Monomial` and `Polynomial` — used by the factoring, expansion, and polynomial solving passes.

---

## Overview

A **polynomial** is a sum of **monomials** with numeric coefficients:

$$P(x, y, \ldots) = \sum_i c_i \cdot m_i$$

where each monomial $m_i$ is a product of variables raised to non-negative integer powers:

$$m = x^{a} \cdot y^{b} \cdot z^{c} \cdots$$

Both types maintain **canonical form** through sorted storage and automatic cleanup.

---

## Monomial

A monomial is a product of variables with integer exponents.

### Data

```cpp
class Monomial {
    std::map<std::string, int> vars_;   // variable → exponent (sorted by name)
};
```

The `std::map` ensures alphabetical variable ordering. Zero-exponent entries are always pruned — an empty map represents the constant monomial $1$.

### Constructors

| Constructor          | Result                 | Example                         |
| -------------------- | ---------------------- | ------------------------------- |
| `Monomial()`         | Constant 1 (empty map) | $1$                             |
| `Monomial(var, exp)` | Single variable        | `Monomial("x", 2)` → $x^2$      |
| `Monomial(map)`      | From variable map      | `{{"x", 1}, {"y", 2}}` → $xy^2$ |

If `exp == 0` in a constructor, the variable is not stored (pruned to constant 1).

### Properties

| Method           | Return | Description                                           |
| ---------------- | ------ | ----------------------------------------------------- |
| `total_degree()` | `int`  | Sum of all exponents. Returns 0 for constant.         |
| `degree_of(var)` | `int`  | Exponent of a specific variable. Returns 0 if absent. |
| `is_constant()`  | `bool` | True iff `vars_` is empty (monomial = 1).             |

### Arithmetic

| Operation           | Description                        | Edge Cases                                                                       |
| ------------------- | ---------------------------------- | -------------------------------------------------------------------------------- |
| `a * b`             | Merge exponents (add per variable) | Results with exp=0 are pruned                                                    |
| `a / b`             | Subtract exponents                 | **Caller must verify** `a.divisible_by(b)` — no guard against negative exponents |
| `a.pow(n)`          | Multiply all exponents by `n`      | `pow(0)` → constant 1                                                            |
| `a.divisible_by(b)` | Can `b` divide `a`?                | Returns `false` if any exponent in `b` exceeds corresponding exponent in `a`     |

### Graded Lexicographic Ordering

`operator<` implements **graded lexicographic** (glex) ordering:

1. **Compare total degree** — higher degree sorts first (larger is "less than")
2. **Equal degree → compare per-variable exponent** — higher exponent sorts first

This ensures a canonical term ordering where higher-degree terms appear before lower-degree terms, and among terms of equal degree, the leftmost variable dominates.

| Monomial | Total Degree | Sorted Position                       |
| -------- | ------------ | ------------------------------------- |
| $x^3$    | 3            | 1st (highest)                         |
| $x^2 y$  | 3            | 2nd (same degree, $x$ has higher exp) |
| $x y^2$  | 3            | 3rd                                   |
| $y^3$    | 3            | 4th                                   |
| $x^2$    | 2            | 5th                                   |
| $xy$     | 2            | 6th                                   |
| $y^2$    | 2            | 7th                                   |
| $x$      | 1            | 8th                                   |
| $y$      | 1            | 9th                                   |
| $1$      | 0            | Last                                  |

### String Representation

`to_string()` rules:
- Exponent 1 is suppressed: $x$ not $x^1$
- Constant monomial: `"1"`
- Variables concatenated: $x^2 y^3$ → `"x^2y^3"`

---

## Polynomial

A polynomial is a map from monomials to coefficients.

### Data

```cpp
class Polynomial {
    std::map<Monomial, double> terms_;   // monomial → coefficient (sorted by glex)
};
```

The `std::map` uses the graded lexicographic ordering of `Monomial` as key comparator, ensuring deterministic term ordering.

### Constructors

| Constructor                                 | Result                    | Notes                                               |
| ------------------------------------------- | ------------------------- | --------------------------------------------------- |
| `Polynomial()`                              | Zero polynomial           | Empty `terms_`, `is_zero() == true`                 |
| `Polynomial(double c)`                      | Constant polynomial       | $\left\lvert c \right\rvert < kEpsilon$ → zero poly |
| `Polynomial(double c, string var, int exp)` | Single term $c \cdot v^e$ | $\left\lvert c \right\rvert < kEpsilon$ → zero poly |
| `Polynomial(double c, Monomial m)`          | Single term $c \cdot m$   | $\left\lvert c \right\rvert < kEpsilon$ → zero poly |

All constructors with coefficient values apply the `kEpsilon` threshold to prevent near-zero terms.

### Properties

| Method               | Return   | Description                                                  |
| -------------------- | -------- | ------------------------------------------------------------ |
| `is_zero()`          | `bool`   | `terms_` is empty                                            |
| `is_constant()`      | `bool`   | Zero or single constant-monomial term                        |
| `constant_value()`   | `double` | Coefficient of the constant monomial; 0.0 if absent          |
| `coefficient(m)`     | `double` | Coefficient of monomial `m`; 0.0 if absent                   |
| `degree()`           | `int`    | Maximum total degree across all terms; 0 for zero polynomial |
| `is_univariate()`    | `bool`   | ≤ 1 distinct variable across all terms                       |
| `single_variable()`  | `string` | Name of the single variable; returns `"x"` if none           |
| `coeff_of_degree(n)` | `double` | Coefficient of the degree-`n` term; 0.0 if none              |

### Arithmetic Operators

All operators return **new** `Polynomial` instances. `cleanup()` is called automatically after every operation.

| Operator   | Description                                | Complexity                                                  |
| ---------- | ------------------------------------------ | ----------------------------------------------------------- |
| `a + b`    | Merge term maps, add coefficients          | $O(n + m)$                                                  |
| `a - b`    | Merge term maps, subtract coefficients     | $O(n + m)$                                                  |
| `-(a)`     | Negate all coefficients                    | $O(n)$                                                      |
| `a * b`    | Distribute all pairs                       | $O(n \cdot m)$                                              |
| `a * s`    | Scale by scalar `s`                        | $O(n)$; $\left\lvert s \right\rvert < kEpsilon$ → zero poly |
| `a / s`    | Divide by scalar (converts to `a * (1/s)`) | $O(n)$                                                      |
| `a.pow(n)` | Binary exponentiation                      | $O(n \cdot \log(\text{exp}))$; `pow(0)` → constant 1        |

#### Multiplication Detail

Polynomial multiplication distributes every term of `a` against every term of `b`:

```
For each (mono_a, coeff_a) in a.terms_:
    For each (mono_b, coeff_b) in b.terms_:
        result[mono_a * mono_b] += coeff_a * coeff_b
```

The resulting map is then cleaned up to remove near-zero terms.

#### Binary Exponentiation

`pow(n)` uses repeated squaring:

```
result = Polynomial(1.0)    // identity
base = *this
while n > 0:
    if n is odd:  result = result * base
    base = base * base
    n = n / 2
return result
```

### cleanup()

Called after every arithmetic operation. Removes terms where $|\text{coeff}| < \epsilon$:

```
For each (monomial, coefficient) in terms_:
    If |coefficient| < kEpsilon:
        Erase from map
```

Uses the global `kEpsilon` tolerance. This prevents floating-point noise from creating spurious terms.

---

## GCD Operations

### `coefficient_gcd()` → `double`

Computes the GCD of all numeric coefficients (integers only):

1. Check every coefficient is an integer (within `kCoeffTol`):
   - If `|coeff − round(coeff)| > kCoeffTol` for any term → return `1.0`
2. Round all coefficients to integers, take absolute values
3. Compute GCD using `std::gcd` across all terms
4. Return result as `double`

**Example:** $6x^2 + 9x + 3$ → `coefficient_gcd() = 3`

### `monomial_gcd()` → `Monomial`

Finds the greatest common monomial factor:

1. Start with the first term's variable map
2. For each subsequent term:
   - **Intersect** variable sets (drop variables not present in this term)
   - For common variables, take the **minimum** exponent
3. Return the result as a `Monomial`

**Example:** $x^3 y^2 + x^2 y^3 + x^2 y^2$ → `monomial_gcd() = x²y²`

### `divide_by_monomial(m)` → `Polynomial`

Divides every term by monomial `m`:

```
For each (mono, coeff) in terms_:
    new_terms[mono / m] = coeff
```

**Precondition:** Every term must be divisible by `m`. No runtime guard — the caller must ensure this (typically via `monomial_gcd()`).

---

## String Representation

`to_string()` formatting rules:

| Coefficient | Monomial    | Output                  |
| ----------- | ----------- | ----------------------- |
| `1.0`       | $x^2$       | `"x^2"` (1 suppressed)  |
| `-1.0`      | $xy$        | `"-xy"` (−1 shown as −) |
| `3.0`       | $x$         | `"3x"`                  |
| `2.5`       | $x^2y$      | `"2.5x^2y"`             |
| `5.0`       | constant    | `"5"`                   |
| any         | (zero poly) | `"0"`                   |

Sign handling:
- First term: negative shown as `"-"`, positive has no prefix
- Subsequent terms: `" + "` or `" - "` with space padding

Trailing zeros stripped from decimal coefficients.

---

## Further Reading

- [AST → Polynomial](ast-to-poly.md) — How AST expressions are lowered to `Polynomial`
- [Factor](factor.md) — Factorization using GCD extraction and quadratic decomposition
- [Polynomial Solver](poly-solver.md) — Root-finding on univariate polynomials
- [Tolerance](tolerance.md) — `kEpsilon` and `kCoeffTol` constants
