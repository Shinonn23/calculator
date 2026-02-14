
# Math Solver

An interactive command-line math tool built in C++17. It can evaluate arithmetic expressions, store variables, solve linear equations, and simplify equations to canonical form — all from a REPL with history, completions, and color output.

---

## Installation

See [INSTALL.md](INSTALL.md) for platform-specific setup instructions (Windows and Linux).

---

## Quick Start

```bash
cmake -S . -B build -G Ninja -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
ninja -C build
```

Run the program:

```bash
# Windows
.\build\bin\math-solver.exe

# Linux
./build/bin/math-solver
```

You can also evaluate a single expression directly from the command line:

```bash
./build/bin/math-solver "2 + 3 * 4"
```

---

## How It Works

### Overview

Math Solver is a REPL (Read-Eval-Print Loop) application. The user types a math expression or command, the program processes it, and prints the result. The processing pipeline is:

```
Input string
  |
  v
[ Lexer ] --- tokenize characters into tokens (numbers, operators, identifiers)
  |
  v
[ Parser ] --- build an Abstract Syntax Tree (AST) from tokens
  |
  v
[ Evaluator / Solver / Simplifier ] --- compute result, solve equation, or simplify
  |
  v
Output
```

### Pipeline Detail

1. **Lexer** (`core/lexer/`) — Scans the input string character by character and produces a stream of tokens: `Number`, `Identifier`, `Plus`, `Minus`, `Mul`, `Div`, `Pow`, `LParen`, `RParen`, `Equals`.

2. **Parser** (`core/parser/`) — Consumes the token stream using a recursive descent parser with precedence climbing. Produces either an `Expr` (expression tree) or an `Equation` (two expressions separated by `=`).

3. **AST** (`core/ast/`) — The expression tree is built from these node types:
   - `Number` — a numeric literal (e.g. `42`, `3.14`)
   - `Variable` — a named variable (e.g. `x`, `mass`)
   - `BinaryOp` — a binary operation (`+`, `-`, `*`, `/`, `^`) with left and right children
   - `Equation` — two expressions representing `lhs = rhs`

4. **Evaluator** (`core/eval/`) — Walks the AST using the Visitor pattern and computes a numeric result. Variables are resolved from a `Context` (a name-to-value map).

5. **Solver** (`core/solve/solver.hpp`) — Solves linear equations with one unknown. Collects coefficients from both sides into a `LinearForm` (`ax + b = 0`), then solves for `x = -b/a`.

6. **Simplifier** (`core/solve/simplify.hpp`) — Reduces equations to canonical linear form (e.g. `2x + 3y = 7`). Supports fraction display, variable ordering, and context-aware simplification.

### Context and Variables

The `Context` class stores variable bindings as a `map<string, double>`. Variables can be set via `:set`, solved equations are automatically stored, and the context persists across the session.

### Environments

Environments allow saving and loading named sets of variables. They are serialized to a JSON config file stored in:

| Platform | Path                     |
| -------- | ------------------------ |
| Windows  | `%APPDATA%\math-solver\` |
| Linux    | `~/.config/math-solver/` |

---

## REPL Commands

| Command                     | Description                               |
| --------------------------- | ----------------------------------------- |
| `<expression>`              | Evaluate an expression (e.g. `2 + 3 * x`) |
| `<lhs> = <rhs>`             | Check if equation is true or false        |
| `solve <lhs> = <rhs>`       | Solve a linear equation for the unknown   |
| `simplify <eq> [flags]`     | Simplify equation to canonical form       |
| `:set <var> <value>`        | Set a variable to a value or expression   |
| `:set <var> solve <eq>`     | Solve and store the result in a variable  |
| `:unset <var>`              | Remove a variable                         |
| `:vars`                     | List all defined variables                |
| `:clear`                    | Clear all variables                       |
| `:config list`              | Show all settings                         |
| `:config get <key>`         | Get a setting value                       |
| `:config set <key> <value>` | Change a setting                          |
| `:config reset`             | Reset settings to defaults                |
| `:env list`                 | List saved environments                   |
| `:env save <name>`          | Save current variables as an environment  |
| `:env load <name>`          | Load an environment                       |
| `:env new <name>`           | Create a new empty environment            |
| `:env delete <name>`        | Delete an environment                     |
| `:help`                     | Show help                                 |
| `exit` / `quit`             | Exit the program                          |

### Simplify Flags

| Flag         | Effect                            |
| ------------ | --------------------------------- |
| `--vars x y` | Set variable display order        |
| `--isolated` | Ignore context variables          |
| `--fraction` | Display coefficients as fractions |

### Settings

| Key             | Default     | Description                     |
| --------------- | ----------- | ------------------------------- |
| `precision`     | `6`         | Decimal places in output (0–15) |
| `fraction_mode` | `false`     | Default fraction display        |
| `history_size`  | `1000`      | Max REPL history entries        |
| `auto_load_env` | `"default"` | Environment loaded on startup   |

---

## Project Structure

```
math-solver/
├── src/
│   ├── main.cpp                    # Entry point, REPL loop, command dispatch
│   ├── ast/                        # Expression tree nodes
│   │   ├── expr.hpp                #   Base Expr class + Visitor interface
│   │   ├── number.hpp              #   Numeric literal node
│   │   ├── variable.hpp            #   Variable reference node
│   │   ├── binary.hpp              #   Binary operation node (+, -, *, /, ^)
│   │   └── equation.hpp            #   Equation node (lhs = rhs)
│   ├── lexer/                      # Tokenizer
│   │   ├── token.hpp               #   Token types and struct
│   │   └── lexer.hpp               #   Character-level scanner
│   ├── parser/                     # Recursive descent parser
│   │   ├── parser.hpp              #   Parser class definition
│   │   └── parser.cpp              #   Parsing implementation
│   ├── eval/                       # Expression evaluator
│   │   ├── context.hpp             #   Variable storage (name -> value)
│   │   ├── evaluator.hpp           #   Evaluator class (Visitor)
│   │   └── evaluator.cpp           #   Evaluation logic
│   ├── solve/                      # Equation solving and simplification
│   │   ├── linear_collector.hpp    #   Collect linear coefficients from AST
│   │   ├── solver.hpp              #   Linear equation solver
│   │   └── simplify.hpp            #   Canonical form simplifier
│   └── common/                     # Shared utilities
│       ├── command.hpp             #   REPL command implementations
│       ├── config.hpp              #   Persistent JSON configuration
│       ├── environment.hpp         #   Environment save/load
│       ├── error.hpp               #   Error types with source spans
│       ├── color.hpp               #   ANSI color codes
│       ├── fraction.hpp            #   Fraction formatting
│       ├── replxx.hpp              #   Replxx setup (completions, hints)
│       ├── suggest.hpp             #   Fuzzy suggestion for typos
│       ├── span.hpp                #   Source position tracking
│       └── utils.hpp               #   String utilities
├── .vscode/                        # VS Code configuration
│   ├── settings.json               #   clangd + clang-format settings
│   └── extensions.json             #   Recommended extensions
├── CMakeLists.txt                  # Build configuration
├── INSTALL.md                      # Installation guide
└── README.md
```

---

## Dependencies

| Library                                            | Version | Purpose               |
| -------------------------------------------------- | ------- | --------------------- |
| [nlohmann/json](https://github.com/nlohmann/json)  | v3.11.3 | JSON config storage   |
| [replxx](https://github.com/AmokHuginnsson/replxx) | v0.0.4  | Interactive CLI input |

Dependencies are fetched automatically via CMake `FetchContent` during configuration. No manual installation needed.