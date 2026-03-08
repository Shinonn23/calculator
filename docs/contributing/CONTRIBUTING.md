# Contributing Guide

Thank you for your interest in contributing to **cmath-solver**! This guide covers everything you need to get started.

## Prerequisites

| Tool         | Version       | Notes                            |
| ------------ | ------------- | -------------------------------- |
| C++ compiler | C++17 support | GCC 9+, Clang 10+, or MSVC v143+ |
| CMake        | ≥ 3.16        | Build configuration              |
| Ninja        | any           | Build tool                       |
| Git          | any           | Version control                  |
| Python 3     | any           | UI test runner                   |

Dependencies (nlohmann/json, replxx, GoogleTest) are auto-fetched via CMake FetchContent — no manual installation needed.

For detailed setup instructions, see [INSTALL.md](../../INSTALL.md).

## Building

```bash
# Configure (first time or after adding files to CMakeLists.txt)
cmake -S . -B build -G Ninja -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

# Build
ninja -C build

# Verify
./build/bin/cmath-solver "2 + 3"
# Output: 5
```

## Running Tests

```bash
# All tests (unit + UI)
cd build && ctest

# Unit tests only
./build/bin/ast_tests
./build/bin/parser_tests
./build/bin/algebra_tests

# Specific test filter
./build/bin/ast_tests --gtest_filter="*BinaryExpr*"

# UI tests only
python3 tests/run_ui_tests.py --binary ./build/bin/cmath-solver --tests-dir tests/ui

# Regenerate expected UI output after intentional changes
python3 tests/run_ui_tests.py --binary ./build/bin/cmath-solver --tests-dir tests/ui --bless
```

See [Testing Guide](testing.md) for details on test structure and patterns.

## Development Workflow

### 1. Fork & Branch

```bash
git checkout -b feature/my-feature
```

Use descriptive branch names:
- `feature/add-cot-function`
- `fix/parser-implicit-mul-edge-case`
- `docs/algebra-walkthrough`

### 2. Make Changes

Follow the code style conventions below. If you're adding a new feature, check the step-by-step guides:

- [Adding a Command](adding-a-command.md)
- [Adding an AST Node](adding-an-ast-node.md)
- [Adding a Math Function](adding-a-math-function.md)

### 3. Test

Run the full test suite before committing:

```bash
ninja -C build && cd build && ctest
```

Add tests for all new functionality:
- Unit tests for new AST nodes, parser logic, algebra algorithms
- UI tests (`.msl` scripts) for user-facing behavior changes

### 4. Update Documentation

Every code change should be accompanied by documentation updates:

- **`README.md`** — Update if commands or user-facing behavior changes
- **`INSTALL.md`** — Update if dependencies or build steps change
- **`docs/`** — Update or create architecture/reference docs as appropriate

### 5. Commit

Write clear commit messages:

```
<type>: <short description>

<optional body with more detail>
```

Types: `feat`, `fix`, `docs`, `test`, `refactor`, `chore`

Examples:
```
feat: add cot() function
fix: handle implicit mul after closing paren
docs: add algebra layer walkthrough
test: add UI tests for polynomial factoring
```

### 6. Submit PR

- Ensure all tests pass
- Include description of what changed and why
- Reference any related issues

---

## Code Style

### C++17

The project uses C++17. Key conventions:

| Convention        | Detail                                                             |
| ----------------- | ------------------------------------------------------------------ |
| **Standard**      | C++17 (`-std=c++17`)                                               |
| **Namespace**     | Everything in `math_solver` namespace                              |
| **Header guards** | `#pragma once`                                                     |
| **Pointers**      | `unique_ptr` for ownership, raw pointers for non-owning references |
| **Errors**        | `Result<T>` for user errors, assertions for programmer bugs        |
| **Exceptions**    | Never thrown for user-facing errors                                |
| **Includes**      | Project headers with `""`, system headers with `<>`                |

### Naming

| Entity              | Convention          | Example                              |
| ------------------- | ------------------- | ------------------------------------ |
| Classes / Structs   | PascalCase          | `BinaryOp`, `LinearForm`             |
| Functions / Methods | snake_case          | `parse_expression()`, `evaluate()`   |
| Member variables    | trailing underscore | `left_`, `span_`                     |
| Enums               | PascalCase          | `BinaryOpType::Add`                  |
| Constants           | kCamelCase          | `kEpsilon`, `kPivotTol`              |
| Files               | snake_case          | `math_parser.cpp`, `binary_expr.hpp` |

### File Organization

- **One class per header** for AST nodes and handlers
- **`.hpp` for headers**, `.cpp` for implementation
- **Header-only** for small utilities and inline error factories
- **Keep headers self-contained** — include everything the header needs

### Error Handling Pattern

```cpp
// DO: Return Result<T> for operations that can fail
Result<double> evaluate(const Expr& expr) {
    // ...
    if (error_condition)
        return Result<double>::err(errors::undefined_variable(name));
    return Result<double>::ok(value);
}

// DO: Check Result before using value
auto result = evaluate(expr);
if (result.failed()) {
    sink.push(result.error());
    return;
}
double val = *result;

// DON'T: Throw exceptions for user errors
// throw std::runtime_error("variable not found");  // WRONG
```

---

## Project Structure Quick Reference

```
src/
├── algebra/          # Math algorithms (linear, polynomial, solver, matrix)
├── ast/              # AST node definitions (math + command)
├── commands/         # Command dispatch registry + handlers
├── config/           # JSON config, settings, environments
├── core/             # Shared primitives (Span, Fraction, tolerance)
├── diagnostics/      # Error types + per-subsystem error factories
├── eval/             # Expression evaluator + variable expander
├── lexer/            # Tokenizers (math + command)
├── parser/           # Parsers (math + command subparsers)
├── runtime/          # Runtime state + variable resolver
├── ui/               # REPL, completions, highlighting
└── utils/            # String, path, history utilities

tests/
├── algebra/          # Algebra unit tests
├── ast/              # AST unit tests
├── lexer/            # Lexer unit tests
├── parser/           # Parser unit tests
└── ui/               # UI integration tests (.msl scripts)
```

---

## Further Reading

- [Architecture Overview](../architecture/overview.md) — High-level design
- [Testing Guide](testing.md) — Detailed test instructions
- [Adding a Command](adding-a-command.md) — Step-by-step guide
- [Adding an AST Node](adding-an-ast-node.md) — Step-by-step guide
- [Adding a Math Function](adding-a-math-function.md) — Step-by-step guide
