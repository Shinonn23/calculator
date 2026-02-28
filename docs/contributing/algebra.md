# Contributing to Math Solver — `algebra` Module

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

The `algebra` module owns symbolic manipulation of math ASTs. It contains three
sub-layers: **polynomial** (representing and factoring multivariate polynomial
expressions), **linear** (extracting linear/affine structure from ASTs and
canonicalizing linear equations), and **solver** (solving single-variable linear
equations). The module does not own the AST definitions, the parser, or the
evaluator — it consumes `Expr` nodes produced upstream and returns structured
results (polynomials, linear forms, solutions) downstream. The entry point for
all algebra operations is `src/commands/handlers/math_handler.hpp`, which
dispatches on `MathCommand::Type` and calls into this module.

Dependents: `src/commands/handlers/math_handler.hpp`,
`src/commands/handlers/var_handler.hpp`.

Dependencies: `src/ast/math/` (all `ExprVisitor` implementations traverse AST
nodes), `src/diagnostics/` (all fallible functions return `Result<T>` with
`Diagnostic` errors), `src/runtime/context/context.hpp` (`LinearCollector` and
`EquationSolver` perform context-aware variable substitution).

Internal data flow for polynomial expansion (`:expand`):

```
Expr AST
  │
  └─ ASTToPolynomial::convert()      algebra/polynomial/ast_to_poly.hpp
       └─ ExprVisitor dispatch
            ├─ visit(Number)  → Polynomial(constant)
            ├─ visit(Variable) → Polynomial(1.0, var, 1)
            ├─ visit(UnaryOp)  → negate result
            └─ visit(BinaryOp) → +, -, *, /, ^
                                  └─ Result<Polynomial>
```

Internal data flow for equation solving (`:solve`):

```
Equation AST
  │
  ├─ LinearCollector::collect(lhs)   algebra/linear/linear_collector.hpp
  ├─ LinearCollector::collect(rhs)   algebra/linear/linear_collector.hpp
  │    └─ ExprVisitor dispatch → LinearForm (lhs - rhs)
  │
  └─ EquationSolver::solve()         algebra/solver/solver.cpp
       └─ ax + b = 0 → x = -b/a
            └─ Result<SolveResult>
```

---

## 2. Directory layout

```
src/algebra/
├── polynomial/
│   ├── monomial.hpp        — empty placeholder (Monomial is defined in polynomial.hpp)
│   ├── polynomial.hpp      — Monomial and Polynomial classes; all algebraic operators; cleanup(); header-only
│   ├── ast_to_poly.hpp     — ASTToPolynomial visitor; converts Expr AST to Polynomial; header-only
│   ├── ast_to_poly.cpp     — empty stub (implementation lives in ast_to_poly.hpp)
│   ├── factor.hpp          — FactoredForm, try_factor_quadratic(), factor_polynomial(); header-only
│   └── factor.cpp          — empty stub (implementation lives in factor.hpp)
├── linear/
│   ├── linear_collector.hpp — LinearForm struct; LinearCollector ExprVisitor; header-only
│   ├── linear_collector.cpp — empty stub (implementation lives in linear_collector.hpp)
│   ├── simplify.hpp         — SimplifyOptions, SimplifyResult, Simplifier; header-only
│   └── simplify.cpp         — empty stub (implementation lives in simplify.hpp)
└── solver/
    ├── solver.hpp           — SolveResult struct; EquationSolver class declaration
    └── solver.cpp           — EquationSolver::solve() and solve_for() implementation
```

---

## 3. Core invariants

The following invariants are taken verbatim or closely paraphrased from source
comments. Violating any of them silently corrupts algebraic results.

1. **Monomial nonzero-exponent invariant.** From `polynomial.hpp`:
   _"Exponents are always nonzero; zero exponents are omitted from vars_."_
   Every mutation of `Monomial::vars_` (multiplication, division, construction)
   must prune entries whose value becomes zero.

2. **Monomial constant representation.** From `polynomial.hpp`:
   _"The empty map denotes the constant monomial 1."_
   A `Monomial` with no entries in `vars_` always represents the scalar 1, not
   an error or undefined value.

3. **Monomial total order.** From `polynomial.hpp`:
   _"Used as a key in maps; operator< must be consistent and total."_
   `Monomial::operator<` uses graded lexicographic order (total degree first,
   then per-variable degree descending). Any change to this comparator breaks
   all `std::map<Monomial, …>` containers.

4. **Polynomial no-zero-term invariant.** From `polynomial.hpp`:
   _"Zero coefficients are pruned from terms_."_ and
   _"All algebraic operations must maintain canonical form (no zero terms, no
   duplicate monomials)."_
   Every method that mutates `terms_` must call `cleanup()` before returning.
   `cleanup()` erases any entry whose absolute coefficient is below `1e-12`.

5. **Polynomial floating-point zero threshold.** From `polynomial.hpp`:
   _"Coefficients are double; beware of floating-point precision when checking
   for zero."_
   The threshold is `1e-12` throughout the module (also `kEpsilon` in
   `ast_to_poly.hpp`). Never use `== 0.0` to test coefficient zero.

6. **ASTToPolynomial visitor statelessness.** From `ast_to_poly.hpp`:
   _"The visitor is stateless except for `result_`, which is overwritten on
   each visit. Recursive calls instantiate new visitors to avoid accidental
   state leakage."_
   Do not share a single `ASTToPolynomial` instance across multiple `convert()`
   calls if they interleave.

7. **ASTToPolynomial rejection policy.** From `ast_to_poly.hpp`:
   _"Exponentiation is only allowed for non-negative integer constants. Division
   is only allowed by nonzero constants."_
   Any other form returns `Result<Polynomial>::err(...)`.

8. **FactoredForm normalization invariant.** From `factor.hpp`:
   _"Resulting FactoredForm is normalized: numeric_factor absorbs all constant
   scaling, common_monomial absorbs all monomial GCDs, and factors are
   irreducible under implemented heuristics."_ and
   _"For zero or constant input, factors is empty."_

9. **LinearForm no-negligible-coefficient invariant.** From `linear_collector.hpp`:
   _"coeffs only contains variables with non-negligible coefficients (see
   simplify())."_ and _"constant is always zeroed if sufficiently close to zero."_
   Call `LinearForm::simplify()` after every construction or combination to
   enforce this invariant.

10. **LinearCollector non-linear rejection.** From `linear_collector.hpp`:
    Variable-times-variable, division by variable, and variable raised to a
    non-1 non-0 power all result in an error `Diagnostic` (E0308) stored in
    `error_`. The `collect()` entry point checks `error_` and returns
    `Result<LinearForm>::err(...)`.

11. **SolveResult validity invariant.** From `solver.hpp`:
    _"If has_solution == false, variable and value are not meaningful."_
    Always check `has_solution` before reading `variable` or `value`.

12. **EquationSolver success invariant.** From `solver.cpp`:
    _"result.has_solution is always true on success."_
    `solve()` only populates `SolveResult::has_solution = true` when it
    returns `Result<SolveResult>::ok(result)`.

---

## 4. Common contribution patterns

### 4.1 Add a new polynomial operation

```
Trigger: A new command (e.g., :gcd, :derivative) needs a new polynomial
         transformation that accepts and returns Polynomial.
```

1. **Edit `src/algebra/polynomial/polynomial.hpp`** — add the new method to
   `Polynomial`. Ensure the method calls `cleanup()` before returning any
   `Polynomial` that was constructed by combining terms.

2. **Verify invariant 4** — after each mutation of `terms_`, confirm that
   `cleanup()` is called. The threshold is `1e-12` (matching the existing
   `cleanup()` body):

   ```cpp
   // src/algebra/polynomial/polynomial.hpp
   void cleanup() {
       for (auto it = terms_.begin(); it != terms_.end();) {
           if (std::abs(it->second) < 1e-12)
               it = terms_.erase(it);
           else
               ++it;
       }
   }
   ```

3. **If the operation is exposed as a command**, add a new enumerator to
   `MathCommand::Type` in `src/ast/command/math_command.hpp` (e.g.,
   `Type::Gcd`), then add a `case MathCommand::Type::Gcd:` branch in
   `handle_math()` inside `src/commands/handlers/math_handler.hpp` that
   calls the new function and pushes output via `sink.push_output(...)`.

4. **Build check** — `ninja -C build` must produce zero errors and zero new
   warnings.

---

### 4.2 Extend ASTToPolynomial to support a new node type

```
Trigger: A new AST node type (e.g., a function call node) must be lowerable
         to a Polynomial.
```

1. **Add a `visit()` override** in `src/algebra/polynomial/ast_to_poly.hpp`
   for the new node type. The base class `ExprVisitor` is a closed set — every
   pure-virtual `visit()` must be overridden or the build fails. Pattern from
   existing implementation:

   ```cpp
   // src/algebra/polynomial/ast_to_poly.hpp
   void visit(const UnaryOp& node) override {
       if (error_)
           return;
       node.operand().accept(*this);
       if (error_)
           return;
       switch (node.op()) {
       case UnaryOpType::Neg:
           result_ = result_ * Polynomial(-1.0);
           break;
       }
   }
   ```

2. **Return a `Result<Polynomial>::err(...)` for unsupported constructs** using
   `errors::polynomial()` from
   `src/diagnostics/kinds/polynomial_errors.hpp` (error code E0310). Do not
   throw.

3. **Reset `error_` guard at entry** — check `if (error_) return;` at the top
   of each `visit()` so that a prior error short-circuits the traversal without
   overwriting the first diagnostic.

4. **Build check** — `ninja -C build` and `./build/bin/ast_tests`.

---

### 4.3 Extend LinearCollector to support a new expression form

```
Trigger: A new BinaryOpType or UnaryOpType is added to the AST and the
         linear layer must handle (or explicitly reject) it.
```

1. **Edit `src/algebra/linear/linear_collector.hpp`** — add a `case` in the
   `switch (node.op())` block inside `visit(const BinaryOp& node)` or
   `visit(const UnaryOp& node)`. If the form is non-linear, set
   `error_ = errors::non_linear(...)` using the factory in
   `src/diagnostics/kinds/solver_errors.hpp` (error code E0308) and return
   immediately.

2. **Call `result_.simplify()` in `collect()`** — this is already done at the
   end of `collect()`. New code should not call `simplify()` inside individual
   `visit()` methods; let the entry point handle it.

3. **Verify invariant 9** — confirm that after `collect()` returns, no zero
   coefficients remain in `result_.coeffs` and that `result_.constant` is
   exactly `0.0` if below `1e-12`.

4. **Build check** — `ninja -C build` and `cd build && ctest`.

---

### 4.4 Add a new algebra-backed command (e.g., :factor-all)

```
Trigger: A new user-facing command requires a new algebra operation wired end-
         to-end from command parsing through handler to algebra output.
```

1. **Add the enumerator** to `MathCommand::Type` in
   `src/ast/command/math_command.hpp`.

2. **Register the command name** in
   `src/parser/command/command_parser_registry.cpp` so the parser routes it to
   the correct subparser.

3. **Add the name to `valid_cmds`** in `src/ui/repl/highlighter.hpp` (the set
   is hardcoded by hand — it is never auto-populated).

4. **Implement the handler** as an `inline` function in
   `src/commands/handlers/math_handler.hpp` following the pattern of the
   existing handlers (`do_expand`, `do_factor`, etc.). Use
   `ASTToPolynomial(payload).convert(*expr)` to lower the AST, then call the
   new algebra function. Push output via `sink.push_output(...)` — never write
   directly to `std::cout`.

5. **Add the `case`** in `handle_math()` to dispatch to the new handler.

6. **Add a UI test** in `tests/ui/` as a `.msl` script, run
   `python3 tests/run_ui_tests.py --binary ./build/bin/math-solver --tests-dir tests/ui --bless`
   to generate the expected output file, then commit both.

7. **Build check** — `ninja -C build` and `cd build && ctest`.

---

## 5. Patterns and conventions

### 5.1 Error reporting

All fallible algebra functions return `Result<T>` (defined in
`src/diagnostics/result.hpp`). Never throw for user-visible errors. Attach the
`Span` covering the offending source region. Use the per-subsystem factory
functions:

| Error kind         | Factory function               | Header                                    | Code  |
| ------------------ | ------------------------------ | ----------------------------------------- | ----- |
| Polynomial error   | `errors::polynomial()`         | `diagnostics/kinds/polynomial_errors.hpp` | E0310 |
| Non-linear term    | `errors::non_linear()`         | `diagnostics/kinds/solver_errors.hpp`     | E0308 |
| No solution        | `errors::no_solution()`        | `diagnostics/kinds/solver_errors.hpp`     | E0301 |
| Infinite solutions | `errors::infinite_solutions()` | `diagnostics/kinds/solver_errors.hpp`     | E0302 |
| Multiple unknowns  | `errors::multiple_unknowns()`  | `diagnostics/kinds/solver_errors.hpp`     | E0304 |
| Invalid equation   | `errors::invalid_equation()`   | `diagnostics/kinds/solver_errors.hpp`     | E0303 |
| Division by zero   | `errors::math()`               | `diagnostics/kinds/math_errors.hpp`       | —     |

```cpp
// Good — returns Result with a Diagnostic carrying a Span
if (!right.is_constant()) {
    error_ = errors::polynomial(
        "cannot divide by a variable expression",
        node.right().span(), input_);
    return;
}

// Bad — throws for a user-facing error
throw std::runtime_error("cannot divide by a variable expression");
```

### 5.2 Visitor implementation

`ASTToPolynomial` and `LinearCollector` both implement `ExprVisitor`
(`src/ast/math/expr_visitor.hpp`), a closed set of pure-virtual `visit()`
methods. Every implementation must cover all overloads. Never add a no-op
default to the base class — that would allow silent omissions that surface only
at runtime.

### 5.3 Polynomial mutation and cleanup

Every method that produces a new `Polynomial` by combining terms must call the
private `cleanup()` before returning. This applies to all arithmetic operators
(`+`, `-`, `*`, scalar `*`, `/`) and to any new method added to the class. The
zero threshold is `1e-12` — consistent with `kEpsilon` in `ast_to_poly.hpp` and
the epsilon checks throughout `linear_collector.hpp`.

### 5.4 Header-only implementation

All algebra files except `src/algebra/solver/solver.cpp` are header-only. The
corresponding `.cpp` files are empty stubs kept for build-system consistency.
New algebra code should continue this pattern unless there is a specific link-
time reason to split the implementation. Do not add `#include` guards to the
empty `.cpp` stubs.

### 5.5 Output vs. diagnostics

Push human-readable algebra results via `sink.push_output(text)` inside
handlers. Push `Diagnostic` objects only for errors and warnings. Do not write
directly to `std::cout` from inside any handler or algebra function.

### 5.6 Context-aware variable substitution

`LinearCollector` and `EquationSolver` accept a `const Context*`. When
`context_` is non-null and `isolated_` is false, variable names are
substituted from the context before linear extraction. When `isolated_` is
true, context bindings are skipped and shadowed variable names are recorded in
`shadowed_vars_` for diagnostic use. Pass `nullptr` as the context pointer when
you want purely symbolic (context-free) collection.

---

## 6. Cross-module touch points

Files that must be updated together, derived from actual `#include` relationships
in source.

| Change                                               | Files that must be updated together                                                                                                                                                                                   |
| ---------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Add a new `MathCommand::Type` enumerator             | `src/ast/command/math_command.hpp`, `src/commands/handlers/math_handler.hpp` (new `case` in `handle_math()`), `src/parser/command/command_parser_registry.cpp`, `src/ui/repl/highlighter.hpp`                         |
| Add a new `ExprVisitor` pure-virtual method          | `src/ast/math/expr_visitor.hpp`, `src/algebra/polynomial/ast_to_poly.hpp`, `src/algebra/linear/linear_collector.hpp`, `src/eval/evaluator.cpp` (and any other `ExprVisitor` implementors)                             |
| Add a new `BinaryOpType` or `UnaryOpType`            | `src/ast/math/binary_expr.hpp` or `src/ast/math/unary_expr.hpp`, `src/algebra/polynomial/ast_to_poly.hpp` (add `case`), `src/algebra/linear/linear_collector.hpp` (add `case`), `src/eval/evaluator.cpp` (add `case`) |
| Change `Monomial::operator<`                         | `src/algebra/polynomial/polynomial.hpp` — all code relying on `std::map<Monomial, …>` ordering is affected; re-run all algebra tests                                                                                  |
| Add a new diagnostic error code to the algebra layer | `src/diagnostics/kinds/solver_errors.hpp` or `src/diagnostics/kinds/polynomial_errors.hpp` plus the algebra file that calls the factory                                                                               |
| Wire a new algebra pass into a command handler       | `src/commands/handlers/math_handler.hpp` plus the new algebra header it includes                                                                                                                                      |

---

## 7. Testing checklist

### Build
- [ ] `ninja -C build` produces zero errors and zero new warnings.
- [ ] `./build/bin/ast_tests` passes all existing unit tests.
- [ ] `cd build && ctest` passes all tests (unit + UI).

### Module-specific

- [ ] Every new `Polynomial`-producing function calls `cleanup()` before
      returning (no zero-coefficient terms remain in `terms_`).
- [ ] Every new `Monomial` construction or mutation prunes zero exponents from
      `vars_` (invariant: no zero-valued entries in the exponent map).
- [ ] Every new `ASTToPolynomial` or `LinearCollector` `visit()` override
      covers all new `ExprVisitor` pure-virtual methods (confirmed by a clean
      build — abstract-class errors appear otherwise).
- [ ] Every new fallible algebra function returns `Result<T>` and uses the
      correct per-subsystem error factory (see §5.1 table).
- [ ] Every new algebra pass that is reachable from a user command is wired
      into a `MathCommand::Type` case in
      `src/commands/handlers/math_handler.hpp`.
- [ ] If a new command name was added, it appears in `valid_cmds` in
      `src/ui/repl/highlighter.hpp` (hardcoded set — never auto-populated).
- [ ] `SolveResult::has_solution` is checked before reading `variable` or
      `value` in any new consumer of `EquationSolver`.
- [ ] Context-isolation behavior is tested: verify that passing
      `isolated_ = true` to `LinearCollector` keeps the target variable
      symbolic and records it in `shadowed_variables()`.

### Documentation
- [ ] `README.md` command table updated if user-facing command syntax changed.
- [ ] `docs/commands/<feature>.md` created or updated (run
      `/docs:command <feature>`).
- [ ] This guide's checklist updated if a new contribution pattern was added.
