Generate a project overview document for: $ARGUMENTS

---

## Your role

You are an expert C++17 developer and technical writer for the **Math Solver** project — an interactive REPL built in C++17 with CMake + Ninja. Your job is to produce a clear, accurate overview that a new contributor can use to understand the project from the top down. Every architectural claim must be verified by reading the actual source files before writing.

---

## Project context

The full pipeline is:

```
Raw input string
  │
  ├─ InputRouter::route()                      parser/input_router.hpp
  │    ├─ math expression  → MathParser        parser/math/math_parser.cpp
  │    └─ :command         → CommandParser     parser/command/command_parser.cpp
  │
  ├─ AST node produced
  │    ├─ math:    NumberExpr / VariableExpr / BinaryExpr / EquationExpr
  │    └─ command: XxxCommand (one per command kind)
  │
  ├─ HandlerRegistry::dispatch()               commands/registry.cpp
  │    └─ handler called via CommandVisitor double-dispatch
  │
  └─ Output via DiagnosticSink → sink.flush(std::cout)
```

Key design points:
- **No exceptions for user errors.** All fallible functions return `Result<T>` (`diagnostics/result.hpp`), a `std::variant<T, Diagnostic>`.
- **Visitor pattern** for AST dispatch — `CommandVisitor` in `ast/command/command_visitor.hpp`.
- **Lazy evaluation** — variables store `ExprPtr`, resolved at evaluation time by `runtime/context/resolver.cpp`.
- **JSON-backed config** — settings and environments serialised to `~/.config/cmath-solver/` (Linux) or `%APPDATA%\cmath-solver\` (Windows).

---

## Instructions

### Step 1 — Read the project entry points

Before writing, read these files to ground every claim in actual source:

- `src/main.cpp` — CLI vs. REPL dispatch, argument handling
- `src/parser/input_router.hpp` — how raw input is classified
- `src/lexer/math/math_lexer.hpp` and `src/lexer/command/command_lexer.hpp` — tokenisation
- `src/parser/math/math_parser.hpp` and `src/parser/command/command_parser.hpp` — parsing layer
- `src/ast/math/expr.hpp` — Visitor interface and math node hierarchy
- `src/ast/command/command_visitor.hpp` — command dispatch interface
- `src/commands/registry.cpp` — handler registration and dispatch
- `src/eval/evaluator.hpp` — expression evaluation (Visitor pattern)
- `src/runtime/context/context.hpp` — variable storage
- `src/runtime/context/resolver.hpp` — lazy resolution
- `src/config/config.hpp` and `src/config/settings.hpp` — config and environments
- `src/diagnostics/result.hpp` and `src/diagnostics/diagnostic.hpp` — error system
- `src/ui/repl/repl.hpp` — REPL loop entry point
- `CMakeLists.txt` — build targets (`math_core`, `cmath-solver`, `ast_tests`)

If `$ARGUMENTS` names a specific subsystem (e.g. `algebra`, `runtime`, `ui`), focus the deep-read on that subsystem's directory and summarise the rest at a higher level.

### Step 2 — Write the Overview document

Output to `docs/overview.md` (or `docs/<subsystem>_overview.md` if a subsystem was given) using the template below. Replace every placeholder with real content read from source.

---

## Document template

```markdown
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

One short paragraph: what the program does from a user's perspective.
Follow with a concrete terminal session showing 4–6 representative interactions
(arithmetic, variable, solve, factor, environment). Use a fenced code block
with `text` syntax so it renders as plain terminal output.

---

## 2. Why it exists

One paragraph explaining the purpose: learning project, demonstrates a
complete interpreter pipeline in C++17 (lexer → parser → AST → evaluator).
Mention what concepts it is designed to illustrate.

---

## 3. How to build and run

Exact shell commands (no paraphrase — copy from CMakeLists.txt and INSTALL.md):

### Build
\`\`\`bash
cmake -S . -B build -G Ninja -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
ninja -C build
\`\`\`

### Run
\`\`\`bash
# Interactive REPL
./build/bin/cmath-solver

# Evaluate a single expression and exit
./build/bin/cmath-solver "2 + 3 * 4"

# Run a script file
./build/bin/cmath-solver --script tests/ui/math_solve.msl
\`\`\`

### Tests
\`\`\`bash
cd build && ctest                        # all tests
./build/bin/ast_tests                   # unit tests only
python3 tests/run_ui_tests.py \
  --binary ./build/bin/cmath-solver \
  --tests-dir tests/ui                  # UI tests only
\`\`\`

---

## 4. Pipeline walkthrough

Show the full path from user input to printed output using a concrete example
(e.g. `solve 2*x + 4 = 10`). One sub-section per stage:

### 4.1 InputRouter
### 4.2 Lexer
### 4.3 Parser → AST
### 4.4 Handler dispatch
### 4.5 Math engine
### 4.6 Output / DiagnosticSink

Each sub-section: 2–4 sentences describing what happens at that stage, the
file responsible, and the data type produced at the end of the stage.
Do not paste full source — one representative snippet per stage is enough.

---

## 5. Subsystem map

A table with columns: **Subsystem**, **Directory**, **Responsibility**, **Key files**.

Cover every top-level directory under `src/`:
ast, lexer, parser, eval, algebra, runtime, commands, config, diagnostics, ui, core, utils.

---

## 6. Key design decisions

One paragraph per decision. Confirmed by reading actual source, not inferred:

- Result<T> instead of exceptions — why and where (diagnostics/result.hpp)
- Visitor pattern for AST — why double-dispatch; which visitors exist
- Lazy variable evaluation — ExprPtr storage, resolver.cpp, circular-dep detection
- Modular subparser registry — how new commands are added without touching core
- math_core static library — what it contains and why it is split from main.cpp
- JSON config with FetchContent deps — nlohmann/json, replxx

---

## 7. Where to start reading the code

Ordered reading list for a new contributor, from entry point inward:

1. `src/main.cpp`
2. `src/ui/repl/repl.cpp` and `runner.cpp`
3. `src/parser/input_router.hpp`
4. `src/parser/math/math_parser.cpp` — one complete small feature
5. `src/ast/math/expr.hpp` — Visitor interface
6. `src/eval/evaluator.cpp`
7. `src/commands/registry.cpp` — handler registration pattern
8. `src/diagnostics/result.hpp` — error model

For each entry: one sentence on what to look for and what concept it illustrates.
```

---

### Step 3 — Verify

Re-read the generated document against the source files. Confirm:
- Every file path exists in the repository.
- Every design claim matches actual code (not CMakeLists comments or README prose).
- Code snippets are verbatim or clearly labelled as paraphrased.
- The terminal session in section 1 produces output the program actually generates.

---

## What NOT to do

- Do not invent subsystems or files that do not exist.
- Do not embed line numbers in prose — they go stale.
- Do not copy the README.md prose verbatim; derive from source files instead.
- Do not describe `Result<T>` as exception-based.
- Do not use Thai or any language other than English.
- Do not write section 4 from memory — read `input_router.hpp`, `runner.cpp`, and `registry.cpp` first.
