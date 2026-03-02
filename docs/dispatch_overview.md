# Math Solver — Project Overview

## Table of Contents
1. [What it is](#1-what-it-is)
2. [Why it exists](#2-why-it-exists)
3. [How to build and run](#3-how-to-build-and-run)
4. [Pipeline walkthrough](#4-pipeline-walkthrough)
5. [Subsystem map](#5-subsystem-map)
6. [Key design decisions](#6-key-design-decisions)
7. [Where to start reading the code](#7-where-to-start-reading-the-code)

---

## 1. What it is

**Math Solver** (`cmath-solver`) is an interactive REPL and CLI tool that parses, evaluates, and symbolically manipulates mathematical expressions. The user can enter arithmetic, assign named variables, solve linear equations, factor or expand polynomials, and manage named variable environments — all from a single prompt backed by persistent JSON configuration.

```text
CMath Solver v1.1.5  [default]
Type :help for commands, exit to quit.

[default] > 2 + 3 * 4
14

[default] > :set x = 5
  x = 5

[default] > x^2 + 1
26

[default] > :solve 2*x + 4 = 10
  x = 3

[default] > :factor x^2 - 1
  (x - 1)(x + 1)

[default] > :env save work
  Saved environment 'work' (1 variable)

[default] > exit
```

---

## 2. Why it exists

Math Solver is a learning project that demonstrates how to build a complete interpreter pipeline in modern C++17 — from hand-written lexer through recursive-descent parser, typed AST, Visitor-pattern evaluation, algebra passes, and JSON-backed persistence. It illustrates how to compose these layers cleanly using value-semantics (`Result<T>`), double-dispatch (Visitor), lazy symbolic evaluation, and modular command extensibility — all without a dependency on Boost or a parser-generator framework.

---

## 3. How to build and run

### Build
```bash
cmake -S . -B build -G Ninja -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
ninja -C build
```

### Run
```bash
# Interactive REPL
./build/bin/cmath-solver

# Evaluate a single expression and exit
./build/bin/cmath-solver "2 + 3 * 4"

# Run a script file
./build/bin/cmath-solver --script tests/ui/math_solve.msl
```

### Tests
```bash
cd build && ctest                        # all tests
./build/bin/ast_tests                   # unit tests only
python3 tests/run_ui_tests.py \
  --binary ./build/bin/cmath-solver \
  --tests-dir tests/ui                  # UI tests only
```

---

## 4. Pipeline walkthrough

The following traces `solve 2*x + 4 = 10` through the full pipeline.

### 4.1 Entry and mode selection

`main()` (`src/main.cpp`) inspects `argc`/`argv`. With no arguments it calls `run_repl()`; a `--script` flag calls `run_script_mode()`; any other argument string is passed to `run_cli_mode()`, which invokes the math `Parser` directly. In REPL/script mode `main()` also loads the persistent config (`Config::load()`) and an optional startup environment before handing control to `run_repl()`.

### 4.2 REPL loop and Runner

`run_repl()` (`src/ui/repl/repl.cpp`) configures a `replxx::Replxx` instance (history, completions, syntax highlighting), builds a `HandlerRegistry` via `build_handler_registry()`, then constructs a `Runner` and calls `Runner::run_interactive()`. On each iteration `Runner::run_line()` is called with the trimmed input string; it calls `parse_command()` and dispatches the result. The same `run_line()` path is used by `run_script()` for batch files.

### 4.3 Command parsing — the routing step

`parse_command()` (`src/parser/command/command_parser.cpp`) creates a `CommandParser`, which strips comments and tokenises the input with `CommandLexer`. Routing is determined by the first token:

- **Colon-prefixed token** (e.g. `:solve`) → looked up in a static `SubparserRegistry` (built once by `build_registry()` in `src/parser/command/command_parser_registry.cpp`). The matching `ICommandSubparser` produces a typed `CommandPtr`.
- **Known system keyword without colon** (e.g. `exit`) → `SystemCommandParser` is tried as a legacy fallback.
- **Everything else** → wrapped in a `MathCommand(Type::Evaluate, raw_input)` for arithmetic evaluation.

For `:solve 2*x + 4 = 10` the `MathCommandParser` subparser produces a `MathCommand(Type::Solve, "2*x + 4 = 10")`.

### 4.4 Command AST

`CommandPtr` is a `unique_ptr<Command>`. Every concrete command class (e.g. `MathCommand`, `VarCommand`, `EnvCommand`) inherits from `Command` and implements `accept(CommandVisitor&, DiagnosticSink&)`. The AST for `:solve 2*x + 4 = 10` is a heap-allocated `MathCommand` carrying the expression string and the `Type::Solve` discriminant.

For math sub-expressions the `Parser` (`src/parser/math/math_parser.hpp`) is a separate recursive-descent parser that produces a tree of `ExprPtr` nodes — `Number`, `BinaryOp`, `UnaryOp`, `Variable`, or `EquationExpr` — all inheriting from `Expr` (`src/ast/math/expr.hpp`).

### 4.5 Handler dispatch — double-dispatch via CommandVisitor

`HandlerRegistry::dispatch(cmd, sink)` calls `cmd.accept(*this, sink)`. Because `HandlerRegistry` inherits `CommandVisitor` and implements all eight pure-virtual `visit()` overloads, the virtual call resolves to `HandlerRegistry::visit(const MathCommand&, DiagnosticSink&)`.

That visit method delegates to the `MathReg` sub-registry:

```cpp
// src/commands/registry.cpp
void HandlerRegistry::visit(const MathCommand& cmd, DiagnosticSink& sink) {
    last_command_status_ =
        math_reg_.dispatch(cmd.type(), cmd, ctx_, cfg_, current_env_, sink);
}
```

`CommandRegistry<MathCommand, MathCommand::Type>` is a template dispatch table (`unordered_map<Key, Handler>`). `dispatch()` looks up `MathCommand::Type::Solve` and calls the registered closure, which calls `handlers::handle_math()` in `src/commands/handlers/math_handler.hpp`.

### 4.6 Math engine

`handle_math()` parses the expression string with `Parser`, passes the resulting `EquationExpr` through the linear algebra layer (`src/algebra/solver/solver.cpp`), and produces a solution string. For `:solve 2*x + 4 = 10` the solver reduces to `ax + b = 0` form and returns `x = 3`.

For plain arithmetic (`MathCommand::Type::Evaluate`) the handler uses `Evaluator` (`src/eval/evaluator.cpp`), an `ExprVisitor` that walks the math AST, resolving variables lazily from `Context` via `Resolver::resolve_recursive()`.

### 4.7 Output via DiagnosticSink

Handlers write output by calling `sink.push_output(text)` and errors by calling `sink.push(Diagnostic)`. After `HandlerRegistry::dispatch()` returns, `Runner::run_line()` calls `registry_.sink().flush(std::cout)`, which writes buffered output strings first, then formatted diagnostics (ANSI-coloured, source-annotated). Errors and warnings are collected, deduplicated, sorted by source location, and rendered together.

---

## 5. Subsystem map

| Subsystem            | Directory              | Responsibility                                                         | Key files                                                                                        |
| -------------------- | ---------------------- | ---------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------ |
| **AST — math**       | `src/ast/math/`        | Math expression node hierarchy and `ExprVisitor` interface             | `expr.hpp`, `binary_expr.hpp`, `variable_expr.hpp`, `equation_expr.hpp`, `expr_visitor.hpp`      |
| **AST — command**    | `src/ast/command/`     | Command node hierarchy and `CommandVisitor` interface                  | `command.hpp`, `command_visitor.hpp`, `math_command.hpp`, `var_command.hpp`, `history_entry.hpp` |
| **Lexer — math**     | `src/lexer/math/`      | Tokenises arithmetic expressions into `Token` stream                   | `math_lexer.hpp`, `math_token.hpp`                                                               |
| **Lexer — command**  | `src/lexer/command/`   | Tokenises command lines into `CommandToken` stream                     | `command_lexer.hpp`, `command_token.hpp`, `command_token_stream.hpp`                             |
| **Parser — math**    | `src/parser/math/`     | Recursive-descent parser; precedence climbing                          | `math_parser.hpp`, `math_parser.cpp`                                                             |
| **Parser — command** | `src/parser/command/`  | Command routing and subparser registry                                 | `command_parser.cpp`, `command_parser_registry.cpp`, `subparsers/`                               |
| **Eval**             | `src/eval/`            | Visitor-pattern numeric evaluator; `Expander` for polynomial expansion | `evaluator.hpp`, `evaluator.cpp`, `expander.hpp`                                                 |
| **Algebra**          | `src/algebra/`         | Linear equation solving, polynomial conversion, factor, expand         | `solver/solver.cpp`, `linear/`, `polynomial/`                                                    |
| **Runtime**          | `src/runtime/context/` | Variable store (`Context`), lazy resolver, circular-dep detection      | `context.hpp`, `resolver.hpp`, `validator.hpp`                                                   |
| **Commands**         | `src/commands/`        | `HandlerRegistry`, `CommandRegistry<>` template, and per-kind handlers | `registry.hpp`, `registry.cpp`, `handlers/`                                                      |
| **Config**           | `src/config/`          | JSON-backed settings and named environments                            | `config.hpp`, `settings.hpp`, `environment.hpp`                                                  |
| **Diagnostics**      | `src/diagnostics/`     | `Result<T>`, `Diagnostic`, `DiagnosticSink`, per-subsystem error kinds | `result.hpp`, `diagnostic.hpp`, `sink.hpp`, `kinds/`                                             |
| **UI**               | `src/ui/repl/`         | REPL loop, `Runner`, completions, highlighting, history                | `repl.cpp`, `runner.hpp`, `runner.cpp`, `completions.hpp`, `highlighter.hpp`                     |
| **Core**             | `src/core/`            | Shared primitives used across subsystems                               | `span.hpp`, `fraction.hpp`                                                                       |
| **Utils**            | `src/utils/`           | String helpers, path utilities, history range parsing                  | `string_utils.hpp`, `path_utils.hpp`, `history_range.hpp`                                        |

---

## 6. Key design decisions

### `Result<T>` instead of exceptions

All fallible functions return `Result<T>` (`src/diagnostics/result.hpp`), a thin wrapper around `std::variant<T, Diagnostic>`. This keeps the error path explicit and type-safe: callers must check `if (!result)` or pattern-match before dereferencing. `Diagnostic` carries the human-readable message, an error code string, a source `Span`, an inline label, and optional help/note text. The `DiagnosticSink` collects multiple diagnostics across a single dispatch cycle and renders them together at the end of `flush()`. C++ exceptions are only used for true programming errors (e.g. `assert` failures) — never for user-visible parse or evaluation errors.

### Visitor pattern for AST dispatch

Two independent Visitor hierarchies exist. `ExprVisitor` (`src/ast/math/expr_visitor.hpp`) is implemented by `Evaluator`, `Expander`, and the algebra collection passes; it drives all numeric and symbolic transformations of the math AST. `CommandVisitor` (`src/ast/command/command_visitor.hpp`) declares eight pure-virtual `visit()` overloads — one per concrete command type — and is implemented by `HandlerRegistry`. Adding a new command type requires adding a new `visit()` to the interface, which forces the compiler to flag any incomplete implementation at build time, preventing silent dispatch failures.

### Lazy variable evaluation

`Context` stores `unordered_map<string, ExprPtr>`, not `double` values. When a variable is bound with `:set x = y + 1`, the parser produces an `ExprPtr` subtree and `Context::set()` stores it symbolically. Numeric resolution happens on demand via `Resolver::resolve_recursive()` (`src/runtime/context/resolver.hpp`), which traverses the stored AST while tracking visited variable names in an `unordered_set<string>` to detect and reject cyclic references (e.g. `x = x`). This design allows variables to refer to other variables without eager evaluation.

### Modular subparser registry

`CommandParser::parse()` maintains a **static** `SubparserRegistry` (`std::unordered_map<string, unique_ptr<ICommandSubparser>>`) built once by `build_registry()` in `src/parser/command/command_parser_registry.cpp`. Adding a new command (e.g. `:integrate`) requires only: (1) implement a class deriving `ICommandSubparser`, (2) add a line to `build_registry()`, and (3) add a `visit()` overload to `CommandVisitor` + `HandlerRegistry`. No other files in the parser or dispatch layer need to change.

### `math_core` static library

The `math_core` static library (`CMakeLists.txt`) contains every `.cpp` file except `src/main.cpp`. The `cmath-solver` executable links `math_core` plus `replxx`. The `ast_tests` executable bypasses `math_core` entirely — it compiles only the specific `.cpp` files it needs (currently `src/diagnostics/diagnostic.cpp`) alongside the test sources, keeping the unit test build fast and narrowly scoped.

### JSON config with FetchContent dependencies

Three external dependencies are fetched at configure time via CMake `FetchContent`: `nlohmann/json v3.11.3` (config serialisation), `replxx release-0.0.4` (readline-like REPL input with history and completions), and `googletest v1.17.0` (unit tests). All three are declared in `CMakeLists.txt` with `GIT_SHALLOW TRUE` and `FETCHCONTENT_UPDATES_DISCONNECTED ON` for offline builds. Config is persisted as JSON at `~/.config/cmath-solver/config.json` (Linux) or `%APPDATA%\cmath-solver\config.json` (Windows), as resolved by `Config::resolve_config_path()`.

---

## 7. Where to start reading the code

1. **`src/main.cpp`** — See how mode selection works: `--script` vs. CLI args vs. REPL. This is the only place where `Config::load()` and `load_startup_env()` are called, establishing the invariant that all session state flows through these three objects: `Config`, `Context`, `current_env`.

2. **`src/ui/repl/repl.cpp` and `runner.cpp`** — `run_repl()` shows the one-time setup sequence (replxx, completions, history, registry construction). `Runner::run_line()` is the single path that both interactive and batch modes share: parse one line, dispatch, flush.

3. **`src/parser/command/command_parser.cpp`** — The routing hub. Understand the three branches: colon-prefixed command → subparser registry lookup; legacy system keyword → `SystemCommandParser`; everything else → `MathCommand::Evaluate`. This is where all user input is first classified.

4. **`src/parser/command/command_parser_registry.cpp`** — The single authoritative list of which command strings map to which `ICommandSubparser`. Reading this file tells you every command the program recognises and which parser handles it.

5. **`src/ast/math/expr.hpp`** — The `Expr` base class and `ExprPtr` alias. The `accept(ExprVisitor&)` pure virtual method is the hook for all math passes. Understanding why `clone()` exists (AST rewriting) sets up the mental model for the algebra layer.

6. **`src/commands/registry.hpp`** — The template `CommandRegistry<Cmd, Key>` and `HandlerRegistry`. Read the `dispatch(cmd, sink)` method and the typed `visit()` overloads together to see how double-dispatch flows from a `Command` node to a concrete handler closure.

7. **`src/eval/evaluator.cpp`** — A complete end-to-end `ExprVisitor` implementation. The `visit(const Variable&)` case shows lazy resolution: look up in `Context`, detect cycles via `visited_`, then recursively evaluate the stored `ExprPtr`.

8. **`src/diagnostics/result.hpp`** — `Result<T>` is used everywhere. Reading this file first makes every function signature in the codebase immediately legible: a `Result<CommandPtr>` is either a successfully parsed command or a `Diagnostic` explaining why parsing failed.
