Generate a contribution guide for: $ARGUMENTS

---

## Your role

You are an expert C++17 developer and technical writer for the **Math Solver** project. Your job is to produce precise, code-verified contribution guides for individual modules. Every invariant, checklist item, and code snippet must be read directly from source files before writing. Do not invent patterns or paraphrase README prose.

---

## Output path scheme

| `$ARGUMENTS`      | Files to generate                                                                                                                                |
| ----------------- | ------------------------------------------------------------------------------------------------------------------------------------------------ |
| *(empty)*         | `docs/contributing/ast.md`, `docs/contributing/core.md`, `docs/contributing/algebra.md`, `docs/contributing/ui.md`, `docs/contributing/index.md` |
| `ast`             | `docs/contributing/ast.md`                                                                                                                       |
| `core`            | `docs/contributing/core.md`                                                                                                                      |
| `algebra`         | `docs/contributing/algebra.md`                                                                                                                   |
| `ui`              | `docs/contributing/ui.md`                                                                                                                        |
| two or more names | one file per named module + `docs/contributing/index.md`                                                                                         |

Create the `docs/contributing/` directory if it does not exist. Do not overwrite files that cover modules not named in `$ARGUMENTS`.

---

## Project context — dispatch pipeline

```
Raw input string
  │
  ├─ Runner::run_line()                        ui/repl/runner.cpp
  │    └─ parse_command(line)
  │         └─ CommandParser::parse()          parser/command/command_parser.cpp
  │              ├─ CommandTokenStream          lexer/command/
  │              └─ SubparserRegistry lookup   parser/command/command_parser_registry.cpp
  │                   └─ XxxCommandParser::parse()  parser/command/subparsers/
  │
  ├─ HandlerRegistry::dispatch(*cmd)           commands/registry.cpp
  │    └─ cmd.accept(*this, sink)              double-dispatch via CommandVisitor
  │         └─ HandlerRegistry::visit(const XxxCommand&, DiagnosticSink&)
  │
  └─ Output via DiagnosticSink → sink.flush(std::cout)
```

Global invariants every contributor must know:
- **No exceptions for user errors.** All fallible functions return `Result<T>` (`diagnostics/result.hpp`).
- **Visitor pattern.** `ExprVisitor` and `CommandVisitor` are closed sets — adding a node forces updates to every implementation.
- **Highlighter sync.** `ui/repl/highlighter.hpp` contains a hardcoded `valid_cmds` set that must stay in sync with `parser/command/command_parser_registry.cpp`.
- **Polynomial canonical form.** `Polynomial` must never hold zero-coefficient terms; call `cleanup()` after every mutation.
- **Span integrity.** Every AST node must carry a valid `Span` covering its source region exactly.

---

## Step 1 — Read source files for each module

Read ALL files for each module that appears in `$ARGUMENTS` (or all four modules if no argument). Do not skip any file. Do not write documentation for a file you have not read.

### Module: `ast`

Read in this order:
- `src/ast/math/expr.hpp`
- `src/ast/math/expr_visitor.hpp`
- `src/ast/math/binary_expr.hpp`, `number_expr.hpp`, `variable_expr.hpp`, `unary_expr.hpp`, `equation_expr.hpp`
- `src/ast/command/command.hpp`
- `src/ast/command/command_visitor.hpp`
- `src/ast/command/math_command.hpp`, `var_command.hpp`, `system_command.hpp`
- `src/eval/evaluator.hpp` (an existing `ExprVisitor` implementation to show the pattern)
- `CMakeLists.txt` (how `ast_tests` links without `math_core`)

### Module: `core`

Read in this order:
- `src/core/span.hpp`
- `src/core/fraction.hpp`
- `src/diagnostics/diagnostic.hpp`
- `src/diagnostics/result.hpp`
- `src/diagnostics/sink.hpp`

### Module: `algebra`

Read in this order:
- `src/algebra/polynomial/monomial.hpp`
- `src/algebra/polynomial/polynomial.hpp`
- `src/algebra/polynomial/ast_to_poly.hpp` and `ast_to_poly.cpp`
- `src/algebra/polynomial/factor.hpp` and `factor.cpp`
- `src/algebra/linear/linear_collector.hpp` and `linear_collector.cpp`
- `src/algebra/linear/simplify.hpp` and `simplify.cpp`
- `src/algebra/solver/solver.hpp` and `solver.cpp`

### Module: `ui`

Read in this order:
- `src/ui/repl/repl.cpp`
- `src/ui/repl/runner.hpp` and `runner.cpp`
- `src/ui/repl/highlighter.hpp`
- `src/ui/repl/completions.hpp`
- `src/ui/repl/hints.hpp`
- `src/ui/repl/history.hpp`
- `src/ui/color.hpp`
- `src/ui/formatters/output_formatter.hpp` and `number_formatter.hpp`
- `src/ui/suggestions.hpp`

---

## Step 2 — Write each module guide

Use the document template below for each module. Every placeholder must be replaced with content read from source.

---

## Document template

```markdown
# Contributing to Math Solver — `<Module>` Module

## Table of Contents
1. [Module overview](#1-module-overview)
2. [Directory layout](#2-directory-layout)
3. [Core invariants](#3-core-invariants)
4. [Common contribution patterns](#4-common-contribution-patterns)
5. [Patterns and conventions](#5-patterns-and-conventions)
6. [Cross-module touch points](#6-cross-module-touch-points)
7. [Testing checklist](#7-testing-checklist)

---

## 1. Module overview

One paragraph: what this module does, what it owns, what it does NOT own.
State which other modules depend on it and which it depends on.
Include an ASCII diagram if the module has a non-trivial internal data flow
(e.g. `ExprVisitor` dispatch or `Result<T>` propagation).

---

## 2. Directory layout

A tree showing every real file in the module directory with a one-line description
derived from reading the file. Do not invent files.

```
src/<module>/
├── foo.hpp          — <responsibility read from file>
└── bar.cpp          — <responsibility read from file>
```

---

## 3. Core invariants

A numbered list. Pull each invariant from an actual `// Invariant:`,
`// Correctness relies on:`, or equivalent comment in the source.
Quote the comment verbatim where self-explanatory; paraphrase only when
the comment needs additional context. Do not invent invariants.

Module-specific minimum sets:

**`ast`**: ExprVisitor and CommandVisitor closed-set invariant; `Span` validity;
`clone()` deep-copy contract; `accept()` double-dispatch contract.

**`core`**: `Span` half-open interval invariant; `Result<T>` dereference precondition;
`DiagnosticSink` flush ordering (outputs before diagnostics).

**`algebra`**: `Polynomial` no-zero-term invariant; `Monomial` no-zero-exponent
invariant; `Monomial::operator<` total order; coefficient zero threshold (1e-12).

**`ui`**: `valid_cmds` sync invariant (hardcoded, never auto-populated);
single-pass highlighter (no heap allocations per character); `run_repl()` setup order;
history persistence on exit.

---

## 4. Common contribution patterns

One sub-section per the most-common change a contributor makes to this module.
Each sub-section is an ordered checklist. Every step names a real file.

### 4.x <Pattern name>

```
Trigger: <What makes a contributor need this pattern>
```

1. **Create / edit `<file>`** — <what to do and what contract to satisfy>.
2. **Update `<file>`** — <what to add and why it is required>.
3. **Update every `<interface>` implementation** — list each real implementor found by
   reading the source (e.g. for ExprVisitor: `eval/evaluator.cpp`, `eval/expander.hpp`,
   and any algebra collector that walks the AST).
4. **Verify `<invariant>`** — quote the invariant and state how to check it.
5. **Build check** — `ninja -C build` and `./build/bin/ast_tests`.

Include one representative code snippet showing the pattern (verbatim from source,
not invented). Use `// src/<path>` as the comment on the first line.

---

## 5. Patterns and conventions

### 5.1 Error reporting

Always return `Result<T>` from fallible functions; never throw for user-visible
errors. Attach a `Span` covering the offending source region. Use the
per-subsystem factory functions in `src/diagnostics/kinds/<subsystem>_errors.hpp`.

```cpp
// Good
return Result<T>::err(errors::parse("unexpected token", span, input));

// Bad — throws an exception for a user-facing parse error
throw std::runtime_error("unexpected token");
```

### 5.2 Visitor implementation

Every `ExprVisitor` implementation must cover all pure-virtual `visit()` overloads.
Never add a no-op default to the base class — that would allow silent omissions.
The same rule applies to `CommandVisitor`.

### 5.3 Span propagation

Every AST node constructor must set `span_` via `Expr(span)`. When a rewriting pass
produces a new node, merge spans with `Span::merge()` so error diagnostics highlight
the full source region.

### 5.4 Output vs. diagnostics

Push human-readable results via `sink.push_output(text)`. Push `Diagnostic` objects
only for errors and warnings. Do not write directly to `std::cout` from inside a handler.

---

## 6. Cross-module touch points

Files that must be updated **together** when making a change. Derive from actual
`#include` relationships found in source — do not guess.

| Change        | Files that must be updated together |
| ------------- | ----------------------------------- |
| <change type> | <list of real files>                |

At minimum cover the changes that are most common for this module.
Pull the list from the `#include` graph, not from general reasoning.

---

## 7. Testing checklist

### Build
- [ ] `ninja -C build` produces zero errors and zero new warnings.
- [ ] `./build/bin/ast_tests` passes all existing unit tests.
- [ ] `cd build && ctest` passes all tests (unit + UI).

### Module-specific
*(add module-specific items derived from reading source)*

For **`ast`**:
- [ ] Every new `ExprVisitor` or `CommandVisitor` implementation covers all
      pure-virtual methods (confirmed by a clean build — no abstract-class errors).
- [ ] Every new AST node sets `span_` in its constructor and implements `clone()`
      as a deep copy (no shallow pointer copies).

For **`core`**:
- [ ] Every new `Result<T>`-returning function is checked for error before dereference.
- [ ] Every new `Diagnostic` uses an error code not already in use in `kinds/`.

For **`algebra`**:
- [ ] Every new `Polynomial`-producing function calls `cleanup()` before returning.
- [ ] Every algebra pass is wired into a `MathCommand::Type` case in `math_handler.hpp`.

For **`ui`**:
- [ ] If a command was added, its name (and any aliases) appear in `valid_cmds` in
      `src/ui/repl/highlighter.hpp`.
- [ ] The command is registered in `build_registry()` in
      `src/parser/command/command_parser_registry.cpp`.

### Documentation
- [ ] `README.md` command table updated if user-facing syntax changed.
- [ ] `docs/commands/<feature>.md` created or updated (run `/docs:command <feature>`).
- [ ] This module's contribution guide updated if the pattern checklist changed.
```

---

## Step 3 — Generate the index file (when multiple modules are written)

When two or more module files are generated, also write `docs/contributing/index.md`:

```markdown
# Contributing to Math Solver

This directory contains per-module contribution guides.

| Module  | File                     | One-line summary                             |
| ------- | ------------------------ | -------------------------------------------- |
| AST     | [ast.md](ast.md)         | Math and command AST nodes, Visitor pattern  |
| Core    | [core.md](core.md)       | Span, Fraction, Result<T>, DiagnosticSink    |
| Algebra | [algebra.md](algebra.md) | Polynomial, linear solver, factoring         |
| UI      | [ui.md](ui.md)           | REPL loop, highlighter, completions, history |

## Global invariants

*(copy the four global invariants from Step 2's Project context section)*

## Quick-start checklist (any module)

- [ ] `ninja -C build` — zero errors, zero new warnings.
- [ ] `cd build && ctest` — all tests pass.
- [ ] `README.md` updated if user-facing syntax changed.
- [ ] `docs/commands/<feature>.md` created or updated.
```

---

## Step 4 — Verify

Re-read every generated file against the source:
- Every invariant is quoted or paraphrased from an actual source comment.
- Every checklist step names a file that exists in the repository.
- Every code snippet is verbatim from the source (or clearly labelled as paraphrased).
- No line numbers appear anywhere in the document (they go stale).
- The cross-module touch-point table reflects actual `#include` relationships.
- Every file path in the directory layout tree is a real file on disk.

---

## What NOT to do

- Do not generate a single monolithic `CONTRIBUTING.md` — always use per-module files in `docs/contributing/`.
- Do not invent files or directories that do not exist under `src/`.
- Do not copy README.md prose verbatim.
- Do not describe `Result<T>` as exception-based.
- Do not omit the `CommandVisitor` / `ExprVisitor` closed-set invariant — it is the most common source of silent breakage.
- Do not embed line numbers anywhere.
- Do not use Thai or any language other than English.
- Do not describe the highlighter's `valid_cmds` as auto-populated — it is a hardcoded set maintained by hand.
- Do not skip the index file when multiple modules are generated.
