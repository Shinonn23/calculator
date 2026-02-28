# Contributing to Math Solver — `core` Module

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

The `core` module owns the primitive types and error-handling infrastructure used by every other subsystem. It is split across two directories: `src/core/` provides `Span` (source position) and `Fraction` (exact rational arithmetic), while `src/diagnostics/` provides `Diagnostic` (structured error/warning), `Result<T>` (monadic error propagation), `DiagnosticSink` (collection and rendering), and per-subsystem error-kind factories in `src/diagnostics/kinds/`. This module does **not** own any AST nodes, evaluation logic, REPL UI, or algebra transforms — those subsystems depend on `core`, not the other way around.

Dependency direction:

```
Every subsystem
    │
    ▼
src/diagnostics/       ←  sink, result, diagnostic, kinds/
    │
    ▼
src/core/              ←  span, fraction
    │
    ▼
(no project-local dependencies)
```

The `DiagnosticSink` flow inside a single handler call:

```
handler receives DiagnosticSink& sink
  │
  ├─ computation returns Result<T>
  │    ├─ Result::ok(value)   →  sink.push_output(formatted_text)
  │    └─ Result::err(diag)   →  sink.push(diag)
  │
  └─ caller calls sink.flush(std::cout)
       ├─ flush_outputs()     (outputs printed first)
       └─ render errors/warnings (sorted, deduplicated)
```

---

## 2. Directory layout

```
src/core/
├── span.hpp            — Span struct: half-open [start, end) into a source buffer;
│                         merge(), length(), empty(); format_error_at_span() /
│                         format_warning_at_span() helpers
└── fraction.hpp        — Fraction struct: exact rational (int64_t num/den), always
                          reduced, denominator always positive; double_to_fraction()
                          continued-fraction converter; format_coefficient() display helper

src/diagnostics/
├── diagnostic.hpp      — Diagnostic struct: message, level, code, inline_label, span,
│                         input, help, note, loc; SourceLocation; find_token_span();
│                         builder methods with_help/note/label/location
├── diagnostic.cpp      — Diagnostic::format() ANSI renderer; Diagnostic::make() factory
├── result.hpp          — Result<T>: std::variant<T,Diagnostic> with ok/failed/map/
│                         map_error/with_location; MultiResult<T>: optional value +
│                         error and warning vectors for batch operations
├── sink.hpp            — DiagnosticSink: staged output (push_output), error/warning
│                         collection with dedup, max_errors cap (default 20), sorting,
│                         flush ordering (outputs → errors → warnings)
└── kinds/
    ├── command_errors.hpp      — errors::unknown_command()  [E0002]
    ├── command_kind.hpp        — command_kind::unknown_subcommand(), missing_arg();
    │                             loc_from_cmd<Cmd>() / raw_from_cmd<Cmd>() helpers
    ├── config_errors.hpp       — errors::missing_setting_key(), missing_key_or_value(),
    │                             unknown_setting(), invalid_setting_value(),
    │                             env_ref_error(), unknown_config_subcommand()
    │                             [E0502, E0503, E0504, E0505]
    ├── env_errors.hpp          — errors::env_not_found(), missing_env_name(),
    │                             unknown_env_subcommand(), var_skipped_warning()
    │                             [E0600, E0601, E0002]
    ├── history_errors.hpp      — errors::history_range_error(), history_missing_arg(),
    │                             history_write_error(), unknown_history_subcommand()
    │                             [E0701, E0702, E0703, E0002]
    ├── math_errors.hpp         — errors::math() [E0000], errors::parse() [E0001]
    ├── polynomial_errors.hpp   — errors::polynomial() [E0310]
    ├── runtime_errors.hpp      — errors::undefined_variable() [E0425]
    ├── runtime_errors_extra.hpp— errors::circular_dependency() [E0391]
    ├── solver_errors.hpp       — errors::no_solution() [E0301], infinite_solutions()
    │                             [E0302], invalid_equation() [E0303],
    │                             multiple_unknowns() [E0304], non_linear() [E0308]
    └── var_errors.hpp          — errors::var_not_found(), missing_var_name(),
                                  reserved_keyword(), invalid_identifier(),
                                  missing_expr()  [E0425, E0401, E0402, E0403, E0404]
```

---

## 3. Core invariants

1. **`Span` half-open interval.** From `src/core/span.hpp`:
   > `// Invariant: [start, end) is a half-open interval into some source buffer. end >= start is assumed by all consumers.`
   `merge()` assumes both spans refer to the same underlying buffer. `length()` performs no bounds checking — callers must ensure `end >= start`.

2. **`Fraction` always reduced, denominator always positive.** From `src/core/fraction.hpp`:
   > `// Invariant: denominator is always positive after construction. Fraction is always stored in reduced form.`
   The constructor calls `simplify()` unconditionally. `simplify()` normalizes a zero denominator to `0/1` to avoid undefined behavior.

3. **`Result<T>` dereference precondition.** `operator*` and `operator->` assert `ok()` before accessing the value. `error()` asserts `failed()` before accessing the `Diagnostic`. Callers must check `Result::ok()` (or `operator bool()`) before dereferencing — violated preconditions abort via `assert`.

4. **`DiagnosticSink` flush ordering — outputs before diagnostics.** `flush()` calls `flush_outputs()` first so that normal result text is printed before any error messages for the same command invocation.

5. **No exceptions for user errors.** All fallible functions return `Result<T>` (`std::variant<T, Diagnostic>`). Never throw for user-visible errors. This invariant applies to every layer that uses this module.

6. **Error code uniqueness.** Each `kinds/` factory function declares a fixed error code string (e.g., `"E0425"`). A new factory must not reuse a code already assigned to a different failure mode. Codes currently in use: `E0000`, `E0001`, `E0002`, `E0301`–`E0304`, `E0308`, `E0310`, `E0391`, `E0401`–`E0404`, `E0425`, `E0502`–`E0505`, `E0600`–`E0601`, `E0701`–`E0703`.

7. **`double_to_fraction()` safe for special floats.** From `src/core/fraction.hpp`:
   > `// Returns 0/1 for NaN or Inf to avoid propagating invalid state.`
   Callers must not rely on exact conversion for very large denominators (the algorithm breaks out of the continued-fraction loop when `k2 > max_denominator`).

---

## 4. Common contribution patterns

### 4.1 Adding a new error kind

```
Trigger: A handler or subsystem needs a new, named user-visible error.
```

1. **Identify the subsystem.** Choose or create the appropriate `src/diagnostics/kinds/<subsystem>_errors.hpp`. Do not put error factories in the header that uses them; keep them in `kinds/`.

2. **Choose an error code.** Check the table in [invariant 6](#3-core-invariants) to confirm the code is not already in use. Assign the next sequential code for the subsystem's range (e.g., a new parse error would be `E0002` or the next available in the `E00xx` range).

3. **Write the factory function** in the `errors` namespace (or `command_kind` namespace for generic command helpers). Use `Diagnostic::make()` for errors and `Diagnostic::warning()` for warnings, then chain builder methods as needed:

   ```cpp
   // src/diagnostics/kinds/var_errors.hpp
   inline Diagnostic missing_var_name(const std::string& raw,
                                      const std::string& cmd_token,
                                      const std::string& usage,
                                      const std::string& file,
                                      size_t             line) {
       auto d = Diagnostic::make("missing variable name", "E0401",
                                 find_token_span(raw, cmd_token), raw,
                                 "variable name expected here")
                    .with_location(file, line);
       d.help = "Usage: " + usage;
       return d;
   }
   ```

4. **Include `find_token_span`** when highlighting a token within the raw input. It is declared in `diagnostic.hpp` and does not require a separate include.

5. **If suggestions are needed**, include `ui/suggestions.hpp` and call `suggest(name, candidates)`. It returns `std::optional<std::string>`.

6. **Call the factory at the handler site** and return `Result<T>::err(errors::your_factory(...))`. Never construct `Diagnostic` inline at call sites — always delegate to `kinds/`.

7. **Build check** — `ninja -C build` to confirm the new header compiles without errors or warnings.

---

### 4.2 Adding a new `Result<T>`-returning function

```
Trigger: A new fallible computation needs to be added to any subsystem.
```

1. **Declare the return type as `Result<T>`** in the function signature. Include `diagnostics/result.hpp`.

2. **Return success** with `Result<T>::ok(value)` or by constructing `Result<T>(value)` implicitly.

3. **Return failure** with `Result<T>::err(errors::your_factory(...))`. Do not throw.

4. **Propagate errors** from nested `Result` calls by checking `if (r.failed()) return Result<U>::err(r.error());` or using `.map()` for transformations:

   ```cpp
   // src/diagnostics/result.hpp
   template <typename F>
   auto map(F&& f) const -> Result<decltype(f(std::declval<T>()))> {
       using U = decltype(f(std::declval<T>()));
       if (ok())
           return Result<U>::ok(f(**this));
       return Result<U>::err(error());
   }
   ```

5. **Attach source location** when propagating across file/line boundaries using `.with_location(file, line)` on a temporary `Result`:
   ```cpp
   return parse_expr(input).with_location(cmd.source_file(), cmd.source_line());
   ```

6. **At the call site (handler)** — check `ok()` before dereferencing:
   ```cpp
   auto result = compute_something(args);
   if (result.failed()) { sink.push(result.error()); return; }
   sink.push_output(format(*result));
   ```

7. **Never dereference without checking** — `operator*` asserts `ok()` and will abort on failure.

---

### 4.3 Adding a new `MultiResult<T>` use case

```
Trigger: A batch operation can produce multiple errors and/or warnings alongside
         a partial result (e.g., loading a script with several malformed lines).
```

1. **Return `MultiResult<T>`** from the batch function. Include `diagnostics/result.hpp`.

2. **Accumulate errors** by pushing to `MultiResult::errors`, and warnings with `.add_warning(diag)`:

   ```cpp
   // src/diagnostics/result.hpp
   MultiResult& add_warning(Diagnostic w) {
       warnings.push_back(std::move(w));
       return *this;
   }
   ```

3. **Check success** via `MultiResult::ok()` — requires both `value.has_value()` and `errors.empty()`.

4. **Push to sink** at the call site — `DiagnosticSink::push(const MultiResult<T>&)` drains both `errors` and `warnings` into the sink in a single call.

---

### 4.4 Adding a new `Span`-based format helper

```
Trigger: A new diagnostic rendering context needs caret or tilde markers.
```

1. **Use `Span::merge()`** when combining spans from child nodes into a parent span:
   ```cpp
   // src/core/span.hpp
   Span merge(const Span& other) const {
       return Span(start < other.start ? start : other.start,
                   end > other.end ? end : other.end);
   }
   ```

2. **Use `format_error_at_span()`** (carets) or `format_warning_at_span()`** (tildes) for low-level text rendering outside of `Diagnostic::format()`. These helpers are inlined in `src/core/span.hpp`.

3. **Zero-width spans** — both helpers emit a single caret/tilde for empty spans (useful for EOF or point-error positions).

4. **Do not set `Span::end < Span::start`** — `length()` performs no bounds check and will wrap on underflow.

---

## 5. Patterns and conventions

### 5.1 Error reporting

Always return `Result<T>` from fallible functions; never throw for user-visible errors. Attach a `Span` covering the offending source region. Use the per-subsystem factory functions in `src/diagnostics/kinds/<subsystem>_errors.hpp`.

```cpp
// Good
return Result<T>::err(errors::parse("unexpected token", span, input));

// Bad — throws an exception for a user-facing parse error
throw std::runtime_error("unexpected token");
```

### 5.2 Output vs. diagnostics

Push human-readable results via `sink.push_output(text)`. Push `Diagnostic` objects only for errors and warnings. Do not write directly to `std::cout` from inside a handler. `DiagnosticSink::flush()` guarantees that outputs are printed before any diagnostic messages for the same command.

```cpp
// Good
sink.push_output(formatted_result + "\n");

// Bad — bypasses sink ordering guarantees
std::cout << formatted_result << "\n";
```

### 5.3 Builder pattern for diagnostics

`Diagnostic`'s `with_help()`, `with_note()`, `with_label()`, and `with_location()` all return new `Diagnostic` copies (they are `[[nodiscard]]` const methods). Chaining is safe because each call creates a fresh copy:

```cpp
// src/diagnostics/diagnostic.hpp
[[nodiscard]] Diagnostic with_help(const std::string& h) const {
    Diagnostic copy = *this;
    copy.help       = h;
    return copy;
}
```

### 5.4 Span propagation

When a rewriting pass produces a new node, merge spans with `Span::merge()` so error diagnostics highlight the full source region. Never leave a `Span` at its default `{0, 0}` for nodes that have real source positions.

### 5.5 `find_token_span` for highlighting

Use `find_token_span(raw, token)` to locate the first occurrence of a token string within the raw input line. It returns an empty `Span()` if the token is not found or is empty — callers must handle this case gracefully (an empty span renders as a single caret at position 0).

### 5.6 `Fraction` arithmetic

`Fraction` stores its invariants through the constructor. Never assign `numerator` or `denominator` directly after construction — call `simplify()` afterwards, or construct a new `Fraction`. The `double_to_fraction()` converter returns `Fraction(0, 1)` for NaN or Inf; callers that need to detect those cases should check the original `double` before conversion.

---

## 6. Cross-module touch points

Files that must be updated **together** when making a change (derived from `#include` relationships in source):

| Change                                              | Files that must be updated together                                                                                                                                             |
| --------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Add a new error code to a `kinds/` file             | `src/diagnostics/kinds/<subsystem>_errors.hpp` (new factory); the handler `.cpp` that calls it; `docs/contributing/core.md` invariant 6 (code table)                            |
| Add a new field to `Diagnostic`                     | `src/diagnostics/diagnostic.hpp`; `src/diagnostics/diagnostic.cpp` (`Diagnostic::format()` and `Diagnostic::make()`); any `kinds/` factory that needs to populate the new field |
| Add a new field to `Span`                           | `src/core/span.hpp`; `src/diagnostics/diagnostic.hpp` (uses `Span`); `src/diagnostics/diagnostic.cpp` (renders span in `format()`)                                              |
| Add a new `DiagnosticSink` option                   | `src/diagnostics/sink.hpp` (`Options` struct and `flush()`); every call site that constructs `DiagnosticSink` with explicit options                                             |
| Add a `kinds/` file that uses `suggest()`           | New `src/diagnostics/kinds/<name>_errors.hpp`; must `#include "ui/suggestions.hpp"` and `#include "diagnostics/diagnostic.hpp"`                                                 |
| Add a `kinds/` file that uses `Config` or `Context` | New header must `#include "config/config.hpp"` or `#include "runtime/context/context.hpp"` respectively (see `config_errors.hpp`, `env_errors.hpp`, `var_errors.hpp`)           |
| Change `Fraction` representation                    | `src/core/fraction.hpp`; `src/algebra/polynomial/monomial.hpp` and any polynomial code that formats coefficients via `format_coefficient()`                                     |

---

## 7. Testing checklist

### Build
- [ ] `ninja -C build` produces zero errors and zero new warnings.
- [ ] `./build/bin/ast_tests` passes all existing unit tests.
- [ ] `cd build && ctest` passes all tests (unit + UI).

### Module-specific

- [ ] Every new `Result<T>`-returning function checks `ok()` or `failed()` before dereferencing the value or accessing the error — confirmed by code review (no unchecked dereferences).
- [ ] Every new `Diagnostic` factory in `kinds/` uses an error code not already listed in [invariant 6](#3-core-invariants). The code table in this document is updated.
- [ ] Every new `Diagnostic` factory is called via `Result<T>::err(errors::your_factory(...))`, not constructed inline at the call site.
- [ ] Any factory that highlights a specific token uses `find_token_span(raw, token)` rather than a hardcoded `Span`.
- [ ] Any factory that offers suggestions includes `ui/suggestions.hpp` and calls `suggest(name, candidates)`.
- [ ] New `Fraction` usage calls the constructor (not direct field assignment) so `simplify()` is always invoked.
- [ ] New `Span` values satisfy `end >= start`; `merge()` is used when combining child spans.
- [ ] `DiagnosticSink` usage calls `push_output()` for normal results and `push(diag)` for errors — never `std::cout` directly from a handler.

### Documentation
- [ ] `README.md` updated if any user-facing error message text or code changed.
- [ ] `docs/commands/<feature>.md` created or updated if the change affects a command's error output.
- [ ] This guide's error-code table (invariant 6) updated if new codes were added.
