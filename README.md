
# Math Solver

A command-line calculator written in C++ that goes beyond basic arithmetic. You can evaluate expressions, store variables, solve equations, and work with polynomials — all inside an interactive terminal session.

**Version**: 1.1.7.1 (2024-06-15)

---

## Why use this?

Most calculators just crunch numbers. Math Solver lets you:

- **Store variables and reuse them** — set `x = 5`, then type `x * 3 + 1` and get `16`
- **Solve equations** — type `:solve 2*x + 4 = 10` and it tells you `x = 3`
- **Solve polynomial equations** — `:solve x^2 = 4` → `x = [-2, 2]`; cubics and higher via Durand–Kerner
- **Broadcast evaluation** — solve `x^3 - 6x^2 + 11x - 6 = 0` → `x = [1, 2, 3]`, then `x^2` → `= [1, 4, 9]`
- **Work symbolically** — set `a = x + 2` and later set `x = 5`; asking for `a` gives `7` automatically
- **Factor and expand polynomials** — `factor x^2 - 5*x + 6` → `(x - 2)(x - 3)`
- **Save your work** — named environments let you save and reload sets of variables across sessions

It is designed as a learning project demonstrating how a real language interpreter is built in C++ (lexer → parser → AST → evaluator pipeline).

---

## Installation

See [INSTALL.md](INSTALL.md) for setup instructions (Linux and Windows).

---

## Quick Start

```bash
# Build
cmake -S . -B build -G Ninja -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
ninja -C build

# Launch the interactive REPL
./build/bin/cmath-solver

# Evaluate a single expression and exit
./build/bin/cmath-solver "2 + 3 * 4"
```

---

## How it works

When you type something, Math Solver runs it through a pipeline:

```
You type:  "solve 2*x + 4 = 10"

1. Input Router   → is this a math expression or a :command?
2. Lexer          → breaks input into tokens: [solve] [2] [*] [x] [+] [4] [=] [10]
3. Parser         → builds a tree structure representing the math
4. Handler        → decides what operation to run (solve, factor, evaluate, etc.)
5. Math Engine    → does the actual algebra / arithmetic
6. Formatter      → prints the result with colors
```

Variables are stored **symbolically** — setting `a = x + 2` stores the expression, not a number. When you later set `x = 5` and ask for `a`, it resolves to `7` at that moment. This is called *lazy evaluation*.

---

## Usage Examples

```
> 2 + 3 * 4
= 14

> :set x 5
x = 5

> x * 3 + 1
= 16

> :solve 2*x + 4 = 10
x = 3 (saved)

> factor x^2 - 5*x + 6
= (x - 2)(x - 3)

> expand (x + 1)^2
= x^2 + 2*x + 1

> simplify 3*x + 2*x = 10
= 5*x = 10
```

### Polynomial solving and broadcast evaluation

```
> :solve x^2 = 4
x = [-2, 2] (saved)

> :solve x^2 + 2*x + 1 = 0
x = -1 (multiplicity 2) (saved)

> :solve x^3 - 6*x^2 + 11*x - 6 = 0
x = [1, 2, 3] (saved)
(solved via Durand-Kerner)

> x^2
= [1, 4, 9]

> :solve x^2 + 1 = 0
error[E0301]: no real solutions (discriminant < 0)
```

When a `:solve` produces multiple roots, the variable is stored as an array. Any subsequent expression using that variable is evaluated for each element — this is called *broadcast evaluation*.

---

## Built-in Functions

Math Solver supports 17 built-in functions that can be used in any expression or equation.

```
> :set pi 3.14159265358979
> sin(pi/2)
= 1

> sqrt(4)
= 2

> :set e 2.71828182845905
> ln(e)
= 1

> :solve sin(pi/6) + x = 1
x = 0.5 (saved)
```

| Category     | Functions                              | Notes                                               |
| ------------ | -------------------------------------- | --------------------------------------------------- |
| Trigonometry | `sin(x)`, `cos(x)`, `tan(x)`           | Argument in radians                                 |
| Inverse trig | `asin(x)`, `acos(x)`, `atan(x)`        | `asin`/`acos` require `x ∈ [-1,1]`                  |
| Hyperbolic   | `sinh(x)`, `cosh(x)`, `tanh(x)`        |                                                     |
| Exponential  | `exp(x)`, `ln(x)`, `log(x)`, `sqrt(x)` | `ln`/`log` require `x > 0`; `sqrt` requires `x ≥ 0` |
| Rounding     | `floor(x)`, `ceil(x)`, `round(x)`      |                                                     |
| Other        | `abs(x)`                               |                                                     |

**Solver behaviour**: functions with constant arguments (e.g. `sin(pi/6)`) are numerically collapsed during equation solving. Functions applied to an unknown variable (e.g. `sin(x)`) are rejected as non-linear with a clear error.

Domain violations (e.g. `sqrt(-1)`, `ln(0)`, `asin(2)`) produce error `E0002`.

---

## Commands Reference

### Command grammar

Math commands accept flags and an expression argument. Two forms are supported:

```
:cmd [flags] <expr>             # Unquoted: flags MUST come before the expression
:cmd [flags] "<expr>" [flags]   # Quoted: flags may appear before OR after the expression
```

Negative-number expressions (e.g. `-5 * x`) are always unambiguous because a leading `-digit` is lexed as a word, not a flag.

### Math operations

| Input                            | What it does                                            |
| -------------------------------- | ------------------------------------------------------- |
| `<expression>`                   | Evaluate (e.g. `2 + 3 * x`)                             |
| `:solve <lhs> = <rhs>`           | Solve an equation (linear, quadratic, or higher-degree) |
| `:solve "<lhs> = <rhs>" [flags]` | Same, with quoted expression (flags allowed after)      |
| `:solve <eq1>; <eq2>; ...`       | Solve a system of linear equations                      |
| `simplify <equation>`            | Collect like terms, canonical form                      |
| `expand <expression>`            | Expand to polynomial form                               |
| `factor <polynomial>`            | Factor a polynomial                                     |

### Variables

| Command            | What it does                      |
| ------------------ | --------------------------------- |
| `:set x 5`         | Set variable `x` to `5`           |
| `:set a x + 2`     | Set `a` to the expression `x + 2` |
| `:set a "x + 2"`   | Same, with quoted expression      |
| `:set y solve ...` | Solve and store the result in `y` |
| `:unset x`         | Remove variable `x`               |
| `:ls`              | List all variables                |
| `:clear`           | Remove all variables              |

### Environments (save/load variable sets)

| Command              | What it does                       |
| -------------------- | ---------------------------------- |
| `:env list`          | Show all saved environments        |
| `:env save <name>`   | Save current variables to `<name>` |
| `:env load <name>`   | Load a saved environment           |
| `:env new <name>`    | Create a new empty environment     |
| `:env delete <name>` | Delete an environment              |

### Settings

| Command                          | What it does               |
| -------------------------------- | -------------------------- |
| `:config list`                   | Show all settings          |
| `:config set precision 4`        | Set decimal precision to 4 |
| `:config set fraction_mode true` | Show results as fractions  |

### History

| Command                           | What it does                                     |
| --------------------------------- | ------------------------------------------------ |
| `:history`                        | Show last 20 commands                            |
| `:history all`                    | Show all commands                                |
| `:history <n>`                    | Show last `n` commands                           |
| `:history <a> <b>`                | Show commands from index `a` to `b`              |
| `:history <a>-<b>`                | Show commands in range `a–b` (dash syntax)       |
| `:history search <pattern>`       | Search history by substring                      |
| `:history save <file> [selector]` | Save history (or a range) to a plain-text file   |
| `:history clear`                  | Clear all history (session + disk)               |
| `:history --errors`               | Filter any of the above to error entries only    |
| `:history --success`              | Filter to successful entries only                |
| `:history --warning`              | Filter to warning entries only                   |
| `:history --info`                 | Filter to info entries only                      |
| `:redo`                           | Re-run the most recent command                   |
| `:redo <n>`                       | Re-run history entry `n`                         |
| `:redo <a>,<b>`                   | Re-run entries `a` and `b`                       |
| `:redo <a>-<b>`                   | Re-run entries `a` through `b` (inclusive)       |
| `:redo <a>,<b>-<c>`               | Re-run entries using mixed list + range selector |

### Scripts

| Command                      | What it does                                        |
| ---------------------------- | --------------------------------------------------- |
| `:load <file>`               | Execute a `.msl` script file in the current session |
| `:load --dry-run <file>`     | Parse and echo lines without executing them         |
| `:load --silent <file>`      | Execute without echoing lines or printing a summary |
| `:load --strict <file>`      | Stop on the first error                             |
| `:load --no-rollback <file>` | Keep state changes even if errors occur             |
| `:load --env <name> <file>`  | Execute the script inside a specific environment    |

### Other

| Command                  | What it does              |
| ------------------------ | ------------------------- |
| `:help` / `:h`           | Show help menu            |
| `:clear` / `:cls`        | Clear the terminal screen |
| `:exit` / `:quit` / `:q` | Quit                      |

---

## Settings

| Key             | Default     | Description                             |
| --------------- | ----------- | --------------------------------------- |
| `precision`     | `6`         | Decimal places shown in output (0–15)   |
| `fraction_mode` | `false`     | Display results as fractions by default |
| `history_size`  | `1000`      | Max number of history entries           |
| `auto_load_env` | `"default"` | Environment loaded automatically        |

Config is saved to `~/.config/cmath-solver/` on Linux or `%APPDATA%\cmath-solver\` on Windows.

---

## Dependencies

Fetched automatically by CMake — no manual installation needed.

| Library       | Purpose                      |
| ------------- | ---------------------------- |
| nlohmann/json | Config and environment files |
| replxx        | History, completion, colors  |

---

## Documentation

Detailed walkthroughs for each feature are in [`docs/`](docs/):

| Feature            | Doc                                                                    |
| ------------------ | ---------------------------------------------------------------------- |
| built-in functions | [`docs/contributing/functions.md`](docs/contributing/functions.md)     |
| math (all)         | [`docs/commands/math_commands.md`](docs/commands/math_commands.md)     |
| polynomial solving | [`docs/solve-polynomial.md`](docs/solve-polynomial.md)                 |
| system solving     | [`docs/solve-system.md`](docs/solve-system.md)                         |
| :set / :unset      | [`docs/commands/var_command.md`](docs/commands/var_command.md)         |
| :config            | [`docs/commands/config_command.md`](docs/commands/config_command.md)   |
| :env               | [`docs/commands/env_command.md`](docs/commands/env_command.md)         |
| :history           | [`docs/commands/history_command.md`](docs/commands/history_command.md) |
| :redo              | [`docs/commands/redo_command.md`](docs/commands/redo_command.md)       |
| :load              | [`docs/commands/load_command.md`](docs/commands/load_command.md)       |
| Architecture       | [`docs/dispatch_overview.md`](docs/dispatch_overview.md)               |
