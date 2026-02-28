# Contributing to Math Solver

This directory contains per-module contribution guides.

| Module   | File                       | One-line summary                                          |
| -------- | -------------------------- | --------------------------------------------------------- |
| AST      | [ast.md](ast.md)           | Math and command AST nodes, Visitor pattern               |
| Core     | [core.md](core.md)         | Span, Fraction, Result\<T\>, Diagnostic, DiagnosticSink   |
| Algebra  | [algebra.md](algebra.md)   | Polynomial, linear collector, solver, factoring           |
| Commands | [commands.md](commands.md) | Command dispatch, HandlerRegistry, subparsers             |
| UI       | [ui.md](ui.md)             | REPL loop, highlighter, completions, hints, history, ANSI |

---

## Global invariants

Every contributor must know these before touching any module:

1. **No exceptions for user errors.** All fallible functions return `Result<T>` (`src/diagnostics/result.hpp`). Never throw for user-visible errors.

2. **Visitor pattern is a closed set.** `ExprVisitor` and `CommandVisitor` are pure-virtual interfaces. Adding a new AST node forces every implementation (evaluator, expander, algebra collectors) to add a matching `visit()` override. A missing override is a compile error — that is intentional.

3. **`valid_cmds` in `highlighter.hpp` is hardcoded — never auto-populated.** Adding a command anywhere without updating `src/ui/repl/highlighter.hpp` causes the highlighter to color it RED. The same applies to `all_commands()` in `src/ui/repl/completions.hpp`.

4. **`Polynomial` must never hold zero-coefficient terms.** Call `cleanup()` after every mutation. A polynomial with a zero-coefficient term violates the canonical form assumed by all algebra passes.

---

## Quick-start checklist (any module)

- [ ] `ninja -C build` — zero errors, zero new warnings.
- [ ] `cd build && ctest` — all tests pass.
- [ ] `README.md` updated if user-facing syntax changed.
- [ ] `docs/commands/<feature>.md` created or updated (run `/docs:command <feature>`).
- [ ] If a command was added: `highlighter.hpp` (`valid_cmds`), `completions.hpp` (`all_commands()`), and `command_parser_registry.cpp` all updated.
- [ ] If a new error kind was added: error code table in `docs/contributing/core.md` updated.
