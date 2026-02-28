# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build & Run

```bash
# Configure (first time or after adding files)
cmake -S . -B build -G Ninja -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

# Build
ninja -C build

# Run REPL
./build/bin/math-solver

# Evaluate single expression
./build/bin/math-solver "2 + 3 * 4"

# Run script file
./build/bin/math-solver --script tests/ui/math_solve.msl
```

## Tests

```bash
# Run all tests (unit + UI)
cd build && ctest

# Run only unit tests (GoogleTest)
./build/bin/ast_tests

# Run a specific GoogleTest filter
./build/bin/ast_tests --gtest_filter="*BinaryExpr*"

# Run only UI tests
python3 tests/run_ui_tests.py --binary ./build/bin/math-solver --tests-dir tests/ui

# Bless (regenerate) expected UI test output after intentional changes
python3 tests/run_ui_tests.py --binary ./build/bin/math-solver --tests-dir tests/ui --bless
```

## Documentation Rule (from `.agent/rules/implements.md`)

Every code change must be accompanied by documentation updates in the same response:
- **`README.md`**: Update if commands or architecture changes
- **`INSTALL.md`**: Update if dependencies or build steps change
- **`docs/<feature>.md`**: Create or update with the Detailed Walkthrough template (Overview → Step-by-step → Checklist)

## Architecture

The pipeline is: `Input → InputRouter → Lexer → Parser → AST → CommandRegistry → Handler → Evaluator/Solver/Algebra → Output`

### Key Subsystems

**Input Processing** (`src/parser/input_router.hpp`): Detects whether raw input is a math expression or a `:command`, routing to the appropriate lexer+parser pair.

**Two Lexer/Parser pairs:**
- Math: `src/lexer/math/` + `src/parser/math/math_parser.cpp` — recursive descent with precedence climbing
- Command: `src/lexer/command/` + `src/parser/command/` — modular subparser registry; each command type (`:set`, `:config`, `:env`, etc.) has its own subparser in `src/parser/command/subparsers/`

**AST** (`src/ast/`): Two families — math nodes (`NumberExpr`, `VariableExpr`, `BinaryExpr`, `EquationExpr`) and command nodes (one per command type). All math nodes implement the Visitor interface defined in `src/ast/math/expr.hpp`.

**Command dispatch** (`src/commands/registry.cpp`): `CommandRegistry` maps command AST nodes to handlers in `src/commands/handlers/`. Handlers call into the algebra/eval layer.

**Math evaluation** (`src/eval/evaluator.cpp`): Visitor-pattern evaluator with lazy variable resolution. Variables store `ExprPtr` (symbolic), not values — resolution happens at evaluation time via `src/runtime/context/resolver.cpp`.

**Algebra layer** (`src/algebra/`):
- `linear/` — collects and simplifies linear equations to canonical form
- `polynomial/` — AST→polynomial conversion, expand, factor
- `solver/` — solves linear equations (`ax + b = 0`)

**Context** (`src/runtime/context/`): `Context` stores `unordered_map<string, ExprPtr>`. `Resolver` performs lazy evaluation with circular-dependency detection. `Validator` checks context integrity. CLI mode uses a temporary context; REPL uses a persistent one.

**Error handling** (`src/diagnostics/`): No exceptions for user errors. All fallible functions return `Result<T>` (a `std::variant<T, Diagnostic>`). `Diagnostic` carries message, error code, source `Span`, inline label, help text, and `SourceLocation`. Error kinds are defined per-subsystem in `src/diagnostics/kinds/`.

**Config & persistence** (`src/config/`): JSON-backed settings and named environments (variable sets). Config stored at `~/.config/math-solver/` on Linux, `%APPDATA%\math-solver\` on Windows. Uses `nlohmann/json` via CMake `FetchContent`.

**REPL** (`src/ui/repl/`): Built on `replxx` (fetched automatically). Provides history, tab-completion, syntax highlighting, and hints.

### Build Architecture

`math_core` static library contains everything except `src/main.cpp`. The `math-solver` executable links `math_core` + `replxx`. The `ast_tests` executable links `GTest::gtest_main` + `nlohmann_json` + `replxx` (does **not** link `math_core` — it compiles `src/diagnostics/diagnostic.cpp` directly).

### UI Test Format

Tests in `tests/ui/` are `.msl` script files (math-solver script language) fed to `--script`. Expected combined stdout+stderr (ANSI-stripped, paths normalized) is stored in the paired `.stderr` file. Run with `--bless` to regenerate expected output after intentional changes.
