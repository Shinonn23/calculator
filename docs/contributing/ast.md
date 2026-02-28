# Contributing to Math Solver — `ast` Module

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

The `ast` module defines every node type used to represent parsed input — both
mathematical expressions and user-typed commands. It owns the two base classes
(`Expr`, `Command`), all concrete node types, the two visitor interfaces
(`ExprVisitor`, `CommandVisitor`), and the supporting `HistoryEntry` type. It
does **not** own parsing, evaluation, algebra, or output formatting: those
responsibilities belong to `parser/`, `eval/`, `algebra/`, and `ui/`
respectively. Nearly every other module in the project depends on `ast/`;
`ast/` itself depends only on `core/` (for `Span`) and `diagnostics/` (for the
`DiagnosticSink` forward-declaration used in `Command::accept()`).

```
               ┌─ ExprVisitor (expr_visitor.hpp) ─────────────────────────┐
               │   visit(Number)   visit(BinaryOp)                        │
               │   visit(UnaryOp)  visit(Variable)                        │
               └──────────────────────────────────────────────────────────┘
                  ▲                ▲                 ▲                 ▲
         Evaluator│       Expander │  ASTToPolynomial│  LinearCollector│
         (eval/)  │       (eval/)  │  (algebra/poly/)│  (algebra/lin/) │

               ┌─ CommandVisitor (command_visitor.hpp) ───────────────────┐
               │   visit(SystemCommand)   visit(VarCommand)               │
               │   visit(MathCommand)     visit(EnvCommand)               │
               │   visit(ConfigCommand)   visit(LoadCommand)              │
               │   visit(HistoryCommand)  visit(RedoCommand)              │
               └──────────────────────────────────────────────────────────┘
                                            ▲
                         HandlerRegistry (commands/registry.hpp)
```

---

## 2. Directory layout

```
src/ast/
├── math/
│   ├── expr.hpp           — Abstract base class Expr: span_, accept(), to_string(), clone()
│   ├── expr_visitor.hpp   — ExprVisitor interface: pure-virtual visit() for all math node types
│   ├── binary_expr.hpp    — BinaryOp node (Add/Sub/Mul/Div/Pow); span merged from children
│   ├── number_expr.hpp    — Number node for numeric literals; value_ must be a finite double
│   ├── variable_expr.hpp  — Variable node; name_ is a raw identifier, resolution deferred
│   ├── unary_expr.hpp     — UnaryOp node (Neg); span-explicit constructor preferred
│   └── equation_expr.hpp  — Equation (not an Expr); owns lhs_ and rhs_, supports take_lhs/rhs
└── command/
    ├── command.hpp         — Abstract base class Command: raw_command_, source location, accept()
    ├── command_visitor.hpp — CommandVisitor interface: pure-virtual visit() for all 8 command types
    ├── history_entry.hpp   — HistoryEntry struct and HistoryStatus enum with stable serialization
    ├── math_command.hpp    — MathCommand: Type enum (Evaluate/Solve/Simplify/Expand/Factor) + flags
    ├── var_command.hpp     — VarCommand: Action enum (Set/Unset) with optional payload/math_action
    ├── system_command.hpp  — SystemCommand: Type enum (Exit/Help/Clear/Ls)
    ├── env_command.hpp     — EnvCommand: Action enum for environment management operations
    ├── config_command.hpp  — ConfigCommand: Action enum (List/Get/Set/Path/Reset) with key/value
    ├── load_command.hpp    — LoadCommand: filepath + Flags (dry_run, silent, strict, no_rollback)
    ├── history_command.hpp — HistoryCommand: Action enum with limit/pattern/filepath/range fields
    └── redo_command.hpp    — RedoCommand: optional range of command indices to redo
```

---

## 3. Core invariants

1. **`Expr::span_` must always be valid and correspond to the source region for
   this node.** Mutations to `span_` are only allowed during construction or
   AST rewriting passes. (Quoted from `src/ast/math/expr.hpp`.)

2. **`ExprVisitor` is a closed set.** "If new node types are added to the AST,
   this interface must be updated accordingly to avoid silent omissions in
   downstream passes." (Quoted from `src/ast/math/expr_visitor.hpp`.) Because
   every `visit()` overload is pure-virtual, a forgotten overload produces a
   compile error rather than a silent no-op.

3. **`CommandVisitor` is a closed set.** "When introducing a new command type,
   `CommandVisitor` must be updated in lockstep to avoid silent dispatch
   failures. This invariant is relied upon by the command dispatch mechanism.
   No default implementations are provided to force explicit handling of each
   command variant." (Quoted from `src/ast/command/command_visitor.hpp`.)

4. **`clone()` performs a deep copy of the subtree, preserving `span_`.**
   "Required for AST rewriting and speculative transformations." A `clone()`
   that leaves `span_` at its default invalidates source mapping in every
   downstream diagnostic. (Quoted/paraphrased from `src/ast/math/expr.hpp` and
   individual node headers.)

5. **`accept()` is the sole entry point for all semantic passes.** "All
   semantic passes (type checking, evaluation, etc.) should use this entry
   point." (Quoted from `src/ast/math/expr.hpp`.) Passes must not downcast or
   inspect node types directly.

6. **`Number::value_` must be a finite double.** "NaN/Inf should be rejected at
   parse time." (Quoted from `src/ast/math/number_expr.hpp`.)

7. **`Equation` is intentionally not an `Expr`.** "This prevents accidental
   nesting of equations within expressions, which would break solver assumptions
   elsewhere in the pipeline." (Quoted from `src/ast/math/equation_expr.hpp`.)

8. **`Equation::lhs_` and `rhs_` are always non-null after construction.** After
   `take_lhs()` or `take_rhs()` is called, the respective accessor must not be
   used. (Paraphrased from the Ownership section of `equation_expr.hpp`.)

9. **`Command::source_file_` is always non-empty and `source_line_` is always
   `>= 1`.** (Quoted from `src/ast/command/command.hpp`.) Call `set_source()`
   before any error reporting that depends on source location.

10. **`HistoryStatus` serialization strings must remain stable across versions
    for on-disk compatibility.** "Any change here must be coordinated with
    `parse_history_status`." (Quoted from `src/ast/command/history_entry.hpp`.)

11. **`BinaryOpType` order must not be changed without auditing all uses.** "The
    order is relied upon by parser and codegen; do not reorder without auditing
    all uses." (Quoted from `src/ast/math/binary_expr.hpp`.)

---

## 4. Common contribution patterns

### 4.1 Adding a new math expression node

```
Trigger: The parser must represent a new mathematical construct (e.g., a
         function call, an absolute-value expression) that has no existing node.
```

1. **Create `src/ast/math/<new_node>.hpp`** — Subclass `Expr`, call
   `Expr(span)` in every constructor, implement `accept()` as
   `visitor.visit(*this)`, implement `to_string()` for diagnostics, and
   implement `clone()` as a deep copy that calls `set_span(span_)` after
   constructing the clone.

2. **Add a forward declaration and a pure-virtual `visit()` overload to
   `src/ast/math/expr_visitor.hpp`** — The new overload must be pure-virtual;
   do not provide a default body.

3. **Update every `ExprVisitor` implementation** — The four current implementors
   that must be updated are:
   - `src/eval/evaluator.hpp` / `src/eval/evaluator.cpp`
   - `src/eval/expander.hpp`
   - `src/algebra/polynomial/ast_to_poly.hpp`
   - `src/algebra/linear/linear_collector.hpp`

4. **Verify the closed-set invariant** — Build with `ninja -C build`. A clean
   build with no abstract-class errors confirms every implementor covers the
   new overload.

5. **Add a unit test in `tests/ast/math/`** — The `ast_tests` executable
   compiles test files directly without linking `math_core`, so the test file
   must not depend on any `math_core`-only translation units.

Representative pattern from `src/ast/math/binary_expr.hpp`:

```cpp
// src/ast/math/binary_expr.hpp
std::unique_ptr<Expr> clone() const override {
    auto cloned = std::make_unique<BinaryOp>(
        left_->clone(), right_->clone(), op_);
    cloned->set_span(span_);
    return cloned;
}
```

---

### 4.2 Adding a new command type

```
Trigger: A new user-typed command (e.g., `:debug`, `:format`) needs its own
         AST node and handler.
```

1. **Create `src/ast/command/<new_command>.hpp`** — Subclass `Command`, call
   `Command(raw)` in the constructor, implement `accept()` as
   `visitor.visit(*this, sink)`.

2. **Add the forward declaration and a pure-virtual `visit()` overload to
   `src/ast/command/command_visitor.hpp`** — Must be pure-virtual with no
   default body.

3. **Update the sole `CommandVisitor` implementor**:
   `src/commands/registry.hpp` (`HandlerRegistry`) and its corresponding
   `src/commands/registry.cpp`.

4. **Register a subparser** — Add an entry in
   `src/parser/command/command_parser_registry.cpp` (`build_registry()`). Add
   a new subparser header under `src/parser/command/subparsers/`.

5. **Register the name in the highlighter** — Add the command name string to
   the hardcoded `valid_cmds` set in `src/ui/repl/highlighter.hpp`. This set
   is never auto-populated.

6. **Verify the closed-set invariant** — `ninja -C build` must produce zero
   abstract-class errors.

Representative `accept()` pattern from `src/ast/command/system_command.hpp`:

```cpp
// src/ast/command/system_command.hpp
void accept(CommandVisitor& visitor,
            DiagnosticSink& sink) const override {
    visitor.visit(*this, sink);
}
```

---

### 4.3 Extending an existing command with a new action variant

```
Trigger: An existing command (e.g., MathCommand, EnvCommand) needs a new
         sub-operation added to its Type/Action enum.
```

1. **Add the variant to the enum** in the relevant command header (e.g.,
   `src/ast/command/math_command.hpp`). Note that `MathCommand::Type::Unknown`
   is reserved for forward-compatibility; do not repurpose it.

2. **Handle the new variant** in `src/commands/registry.cpp` inside the
   `CommandRegistry::dispatch()` call for that command kind.

3. **Update the subparser** in the corresponding file under
   `src/parser/command/subparsers/` to recognize the new keyword and produce
   the new variant.

4. **Add a UI test** in `tests/ui/` with a paired `.stderr` expected-output
   file, then run `python3 tests/run_ui_tests.py --bless` to generate it.

---

## 5. Patterns and conventions

### 5.1 Error reporting

All fallible functions return `Result<T>`; never throw for user-visible errors.
Attach a `Span` covering the offending source region. Use per-subsystem factory
functions in `src/diagnostics/kinds/<subsystem>_errors.hpp`.

```cpp
// Good
return Result<T>::err(errors::parse("unexpected token", span, input));

// Bad — throws an exception for a user-facing parse error
throw std::runtime_error("unexpected token");
```

### 5.2 Visitor implementation

Every `ExprVisitor` implementation must cover all four pure-virtual `visit()`
overloads (`Number`, `BinaryOp`, `UnaryOp`, `Variable`). Every
`CommandVisitor` implementation must cover all eight overloads. Never add a
no-op default to either base class — that would allow silent omissions that
produce incorrect behavior at runtime without a compile-time warning.

### 5.3 Span propagation

Every AST node constructor must set `span_` via `Expr(span)`. When a node
infers its span from children, use `Span::merge()`, as `BinaryOp` and
`Equation` already do. When a rewriting pass produces a new node, call
`set_span(span_)` on the clone so that error diagnostics highlight the
original source region.

For `UnaryOp`: the span-inferring constructor intentionally does not include
the operator token position. Prefer the span-explicit constructor when the
operator source position is available from the parser.

### 5.4 Output vs. diagnostics

Push human-readable results via `sink.push_output(text)`. Push `Diagnostic`
objects only for errors and warnings. Do not write directly to `std::cout`
from inside a handler.

### 5.5 `Equation` is not an `Expr`

Do not attempt to make `Equation` implement `ExprVisitor`-dispatchable, and do
not nest `Equation` inside another expression node. The intentional exclusion
prevents circular solver assumptions. Passes that must inspect both sides
should call `lhs()` and `rhs()` directly, or use `take_lhs()` / `take_rhs()`
to transfer ownership into the solver.

---

## 6. Cross-module touch points

The following files must be updated **together** when making the indicated
change. Relationships are derived from `#include` graphs in source.

| Change                               | Files that must be updated together                                                                                                                                                               |
| ------------------------------------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Add a math AST node                  | `src/ast/math/expr_visitor.hpp`, `src/eval/evaluator.hpp`, `src/eval/evaluator.cpp`, `src/eval/expander.hpp`, `src/algebra/polynomial/ast_to_poly.hpp`, `src/algebra/linear/linear_collector.hpp` |
| Add a command AST node               | `src/ast/command/command_visitor.hpp`, `src/commands/registry.hpp`, `src/commands/registry.cpp`, `src/parser/command/command_parser_registry.cpp`, `src/ui/repl/highlighter.hpp`                  |
| Add a subparser for a new command    | `src/parser/command/command_parser_registry.cpp`, `src/parser/command/subparsers/<new>_command_parser.cpp`, `src/ast/command/<new>_command.hpp`                                                   |
| Add a `MathCommand::Type` variant    | `src/ast/command/math_command.hpp`, `src/commands/registry.cpp` (math handler switch), `src/parser/command/subparsers/math_command_parser.cpp`                                                    |
| Change `HistoryStatus` serialization | `src/ast/command/history_entry.hpp` (`to_string` and `parse_history_status` must stay paired)                                                                                                     |
| Change `BinaryOpType` ordering       | `src/ast/math/binary_expr.hpp` and every site that pattern-matches on `BinaryOpType` (audit with `grep`)                                                                                          |

---

## 7. Testing checklist

### Build
- [ ] `ninja -C build` produces zero errors and zero new warnings.
- [ ] `./build/bin/ast_tests` passes all existing unit tests.
- [ ] `cd build && ctest` passes all tests (unit + UI).

### AST-specific
- [ ] Every new `ExprVisitor` or `CommandVisitor` implementation covers all
      pure-virtual methods (confirmed by a clean build — no abstract-class
      errors).
- [ ] Every new math AST node sets `span_` in its constructor (via `Expr(span)`
      or `set_span()`) and implements `clone()` as a deep copy that calls
      `set_span(span_)` on the cloned object.
- [ ] `Equation` subclasses are not introduced; the intentional exclusion from
      `Expr` must be preserved.
- [ ] `HistoryStatus` string values in `to_string()` and `parse_history_status()`
      remain identical (on-disk compatibility).
- [ ] `BinaryOpType` enum ordering is unchanged unless all dependents have been
      audited.
- [ ] `ast_tests` links without `math_core` — the test target compiles
      `src/diagnostics/diagnostic.cpp` directly and must not pull in any
      translation unit that is only present in `math_core`.

### Documentation
- [ ] `README.md` command table updated if user-facing syntax changed.
- [ ] `docs/commands/<feature>.md` created or updated (run `/docs:command <feature>`).
- [ ] This module's contribution guide updated if the pattern checklist changed.
