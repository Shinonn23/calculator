# Solving Systems of Linear Equations

## Overview

The `:solve` command automatically detects when the payload contains multiple equations separated by `;` and dispatches them to the matrix-based system solver.  No new command is needed — all flags live on `:solve`.

**Supported algorithms:**
- **Gaussian elimination with partial pivoting** (default, `--method=gauss`)
- **LU decomposition** (Doolittle with partial pivoting, `--method=lu`)

## Step-by-step

### 1. Basic system solve

Separate equations with `;`:

```
:solve x + y = 4; x - y = 2
```

Output:
```
  x = 3 (saved)
  y = 1 (saved)
```

Variables are automatically inferred from all equations and sorted alphabetically.  Results are saved to the context unless `--no-save` is given.

### 2. Larger systems

```
:solve x + y + z = 6; 2*x - y + z = 3; x + 2*y - z = 2
```

Output:
```
  x = 1 (saved)
  y = 2 (saved)
  z = 3 (saved)
```

### 3. Flags

| Flag                | Effect                                                              |
| ------------------- | ------------------------------------------------------------------- |
| `--method=gauss`    | Gaussian elimination (default)                                      |
| `--method=lu`       | LU decomposition (P·A = L·U)                                        |
| `--show-matrix`     | Print the augmented matrix `[A\|b]` before solving                  |
| `--rank`            | Print `rank(A)` and `rank([A\|b])` after solving                    |
| `--no-save`         | Solve but do not write results to the context                       |
| `--exact`           | Format solutions as fractions instead of decimals                   |
| `--detect-singular` | Emit a warning if the smallest pivot is less than 1e-6              |
| `--free-vars`       | On infinite solutions: print parameterised form instead of an error |
| `--vars x y z`      | Override variable ordering                                          |

#### `--show-matrix`

```
:solve --show-matrix 2*x + y = 5; x - y = 1
```

Output:
```
  [ A | b ] =
  [ 2   1  |  5 ]
  [ 1  -1  |  1 ]
  x = 2 (saved)
  y = 1 (saved)
```

Columns are right-aligned to the widest entry in each column.

#### `--rank`

```
:solve --rank 2*x + y = 5; x - y = 1
```

Output:
```
  rank(A) = 2, rank([A|b]) = 2
  x = 2 (saved)
  y = 1 (saved)
```

#### `--no-save`

Solve without touching the context:

```
:solve --no-save a + b = 10; a - b = 4
```

Output:
```
  a = 7 (not saved)
  b = 3 (not saved)
```

#### `--exact`

Convert decimal solutions to fractions:

```
:solve --exact 2*x + 3*y = 1; x - y = 0
```

Output:
```
  x = 1/5 (saved)
  y = 1/5 (saved)
```

#### `--free-vars`

When the system is underdetermined (more unknowns than independent equations), print a parameterised form:

```
:solve --free-vars x + y = 4; x + y = 4
```

Output:
```
  Free variables: y
  x = 4 - y
  y = t0
```

### 4. Error cases

**No solution** (E0310) — the system is inconsistent:

```
:solve x + y = 1; x + y = 2
```

**Infinite solutions** (E0311) — the system is underdetermined and `--free-vars` was not given:

```
:solve x + y = 4; x + y = 4
```

Use `--free-vars` to see the parameterised form.

## Checklist

- [x] `;`-separated payload automatically routes to the system solver
- [x] Variable order is inferred (sorted alphabetically) or set via `--vars`
- [x] `--show-matrix` prints a column-aligned augmented matrix
- [x] `--rank` shows rank of A and [A|b]
- [x] `--method=lu` uses Doolittle LU for square systems
- [x] `--exact` / `--fraction` converts results to rational form
- [x] `--no-save` skips context mutation
- [x] `--detect-singular` warns when the smallest pivot is tiny
- [x] `--free-vars` parameterises underdetermined systems
- [x] Error E0310 for inconsistent systems
- [x] Error E0311 for underdetermined systems (without `--free-vars`)
- [x] Single-equation payloads continue to use the existing single-solve path
