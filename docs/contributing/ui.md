# Contributing to Math Solver — `ui` Module

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

The `ui` module owns the interactive REPL shell, syntax highlighting, tab completion, inline hints, history management, ANSI color primitives, fuzzy-match suggestion utilities, and output formatters. It is responsible for everything the user sees and types in the terminal. The module does **not** own any AST nodes, command parsing, evaluation logic, or algebra — those are dispatched through `commands/registry.cpp` after `Runner::run_line()` forwards the raw input string.

Internal data flow:

```
User keystroke
  │
  ├─ replxx callbacks (per-keystroke, O(n) single-pass)
  │    ├─ setup_highlighter()   →  colors[] array in-place
  │    ├─ setup_hints()         →  hint string (only while typing keyword)
  │    └─ setup_completions()   →  completions list (from context + config)
  │
  └─ rx.input() returns a line
       │
       └─ Runner::run_interactive()
            └─ Runner::run_line(line)
                 ├─ parse_command(line)         parser layer
                 └─ registry_.dispatch(*cmd)    command layer
                      └─ sink.flush(std::cout)
```

On exit from `run_repl()`: environment, config, and history are always persisted regardless of session outcome.

---

## 2. Directory layout

```
src/ui/
├── color.hpp                     — ANSI escape constants (reset, bold, dim, red,
│                                   green, yellow, cyan); inline const char*
├── suggestions.hpp               — Levenshtein edit_distance(); suggest() returning
│                                   closest candidate; suggest_all(); maybe_suggest()
│                                   printing "Did you mean X?"; legacy command/setting
│                                   aliases
├── formatters/
│   ├── output_formatter.hpp      — stub (pragma only; reserved for future output
│   │                               formatting helpers)
│   └── number_formatter.hpp      — stub (pragma only; reserved for future numeric
│                                   display helpers)
└── repl/
    ├── repl.hpp                  — run_repl() declaration and its contract/invariant
    │                               comments
    ├── repl.cpp                  — run_repl() implementation: full REPL setup sequence,
    │                               banner, HandlerRegistry construction, Runner dispatch
    ├── runner.hpp                — Runner class: run_interactive(), run_script(),
    │                               run_line(), last_script_had_errors()
    ├── runner.cpp                — Main REPL loop (EINTR/EAGAIN handling), script
    │                               executor with rollback, build_prompt()
    ├── highlighter.hpp           — setup_highlighter(); is_valid_command(); is_operator();
    │                               hardcoded valid_cmds set; single-pass color callback
    ├── completions.hpp           — setup_completions(); all_commands(); config/env
    │                               subcommand lists; detail::complete_config/env/var_names/
    │                               simplify_flags helpers
    ├── hints.hpp                 — setup_hints(); detail::hint_for_keyword(); 300 ms delay
    └── history.hpp               — setup_history() returning resolved history path;
                                    add_history() low-level utility
```

---

## 3. Core invariants

1. **`valid_cmds` sync invariant.** From `src/ui/repl/highlighter.hpp`:
   > `// Invariant: valid_cmds must be kept in sync with the command parser. If a command is added/removed elsewhere, update here.`
   `valid_cmds` is a **hardcoded** `std::unordered_set` — it is never auto-populated from the parser. Adding a command anywhere else without updating this set causes the highlighter to color the new command RED.

2. **Single-pass highlighter, no per-character heap allocations.** From `src/ui/repl/highlighter.hpp`:
   > `// Performance: Single pass, O(n) in input size. No heap allocations except for temporary substrings (could be optimized if necessary).`
   The highlighter callback is invoked on every keystroke; it must remain lightweight. Do not add per-character allocations or nested loops.

3. **REPL setup order.** From `src/ui/repl/repl.cpp`, the required sequence inside `run_repl()` is:
   `setup_history` → `setup_completions` → `setup_hints` → `print_banner` → construct `DiagnosticSink` → `build_handler_registry` → `registry.set_replxx` → `registry.load_persisted_history` → `setup_highlighter` → `Runner::run_interactive`.
   Reordering breaks the requirement that history loading happens after the registry is wired to `replxx`.

4. **History persistence on exit.** From `src/ui/repl/repl.cpp`:
   > `// All persistent state (history, environment, config) must be flushed on normal exit to avoid user data loss.`
   > `// On exit, environment and config are always saved, regardless of session outcome. This is critical for correctness.`
   History is persisted in real time by the registry; `g_config.save()` and `handlers::save_current_env()` must be called unconditionally at the end of `run_repl()`.

5. **Custom history, not replxx built-in.** From `src/ui/repl/repl.cpp`:
   > `// History is managed via a custom mechanism; do not rely on replxx's built-in save/load. This avoids race conditions and ensures consistency with our persistence model.`
   Never call `rx.history_save()` or `rx.history_load()` directly.

6. **Prompt/env-name invariant.** From `src/ui/repl/runner.cpp`:
   > `// Invariant: env_name must always match the active environment. Any change here must be coordinated with completion/hinting logic to avoid desync.`
   `build_prompt(current_env)` is called each iteration; the `current_env` reference is mutated by environment-switch commands.

7. **ANSI reset after every use.** From `src/ui/color.hpp`:
   > `// Invariants: Values must remain valid ANSI codes; consumers must reset formatting after use to avoid leaking styles into unrelated output.`
   Every call site that writes `ansi::bold`, `ansi::red`, etc. must be followed by `ansi::reset` before the next unrelated output.

8. **Completion drift prevention.** From `src/ui/repl/completions.hpp`:
   > `// The logic here must remain consistent with the REPL's command parser; any changes to command syntax must be reflected here to avoid completion drift.`
   `all_commands()` is a separate hardcoded static list from `valid_cmds` in the highlighter. Both must be updated when a command is added or removed.

9. **Runner lifetime invariant.** From `src/ui/repl/runner.hpp`:
   > `// Invariant: registry_ must be valid for the lifetime of this Runner.`
   `Runner` stores a reference to `HandlerRegistry&` — the registry must outlive the `Runner` instance.

---

## 4. Common contribution patterns

### 4.1 Adding a new command

```
Trigger: A new `:command` is added to the parser and command registry.
```

1. **Update `valid_cmds`** in `src/ui/repl/highlighter.hpp` — add the new command string to the hardcoded `std::unordered_set`. Without this, the highlighter colors the new command RED.

   ```cpp
   // src/ui/repl/highlighter.hpp
   static const std::unordered_set<std::string> valid_cmds = {
       ":", ":set", /* ... */ ":your_new_command"};
   ```

2. **Update `all_commands()`** in `src/ui/repl/completions.hpp` — add the command to the hardcoded `static const std::vector<std::string>`. Without this, pressing Tab after the first character does not offer the new command.

3. **Add a hint** in `detail::hint_for_keyword()` in `src/ui/repl/hints.hpp` — map the command string to its argument syntax string. If the command takes no arguments, omit it (the function returns `""` for unknown keywords, which suppresses the hint).

4. **Register in the parser** — add the command to `src/parser/command/command_parser_registry.cpp` `build_registry()`. This step belongs to the `commands` module but is listed here because both registrations must happen together.

5. **Build and verify** — `ninja -C build`, then run `./build/bin/cmath-solver` and type `:your_new_command` to confirm green highlighting, correct Tab completion, and the hint text.

---

### 4.2 Adding subcommand completions for an existing command

```
Trigger: An existing command gains a new subcommand (e.g., `:env` adds a new action).
```

1. **Add to the subcommand list** in `src/ui/repl/completions.hpp` — e.g., `env_subcommands()` or `config_subcommands()`. The list is `static const`; add the new string.

2. **Extend the `complete_*` helper** in `src/ui/repl/completions.hpp` (e.g., `detail::complete_env()`) to handle the new subcommand's argument completions.

3. **Update `detail::hint_for_keyword()`** in `src/ui/repl/hints.hpp` — append the new subcommand to the hint string for the parent command.

4. **Update the error-kind factory** in `src/diagnostics/kinds/` if the handler emits an `unknown_subcommand` diagnostic — the `available` list passed to `command_kind::unknown_subcommand()` must include the new name.

5. **Build check** — `ninja -C build` and `cd build && ctest`.

---

### 4.3 Adding a new suggestion context

```
Trigger: A new handler needs fuzzy "did you mean" suggestions for unrecognized input.
```

1. **Call `suggest()`** from `src/ui/suggestions.hpp` in the relevant `kinds/` factory:

   ```cpp
   // src/ui/suggestions.hpp
   inline std::optional<std::string>
   suggest(const std::string& input,
           const std::vector<std::string>& candidates,
           int max_distance = 2);
   ```

2. **Include `"ui/suggestions.hpp"`** in the `kinds/` header that needs it (see `var_errors.hpp`, `config_errors.hpp`, `env_errors.hpp` for examples).

3. **Do not call `maybe_suggest()`** from inside handlers — that function writes directly to `std::cout`, bypassing the `DiagnosticSink`. Use `suggest()` and set `d.help` instead.

4. **Build check** — `ninja -C build`.

---

### 4.4 Using ANSI colors in new output

```
Trigger: A new formatter or handler needs colored terminal output.
```

1. **Include `"ui/color.hpp"`** — all ANSI constants are in the `ansi` namespace.

2. **Always reset after coloring** — every sequence that opens a style must close it:

   ```cpp
   // src/ui/color.hpp
   // Invariants: consumers must reset formatting after use to avoid leaking
   // styles into unrelated output.
   out += ansi::red;
   out += ansi::bold;
   out += message;
   out += ansi::reset;   // Required — never omit
   ```

3. **Do not add new color constants** unless the new color is unavoidable — the existing six (reset, bold, dim, red, green, yellow, cyan) cover all current use cases. Changes to `color.hpp` affect test harnesses that parse ANSI-stripped output.

---

## 5. Patterns and conventions

### 5.1 No direct `std::cout` from handlers

Route all output through `DiagnosticSink::push_output(text)` so that the sink's flush ordering is respected (outputs before diagnostics). The `Runner` calls `sink.flush(std::cout)` after every `run_line()`. Scripts use a local sink created in `runner.cpp`; use `sink.flush_outputs()` for non-error output in silent mode.

### 5.2 Hardcoded command lists are separate and must both be updated

`valid_cmds` in `highlighter.hpp` and `all_commands()` in `completions.hpp` are two independent hardcoded sets. A command missing from `valid_cmds` is highlighted RED; a command missing from `all_commands()` gets no Tab completion. They are not derived from each other automatically.

### 5.3 Completion callback is performance-sensitive

The completion callback in `setup_completions()` is invoked on every Tab keypress. Keep all completion helpers (`detail::complete_config`, `detail::complete_env`, etc.) free of I/O and heavy computation. All candidate lists must come from in-memory state (`Config::list_envs()`, `Context::all_names()`, static vectors).

### 5.4 Hint callback is more restricted than completions

`setup_hints()` emits a hint **only** while the user is still typing the command keyword (before the first space). It does not complete arguments. Hint text strings in `hint_for_keyword()` are plain strings — do not include ANSI codes.

### 5.5 Script rollback semantics

`Runner::run_script()` takes a `RuntimeSnapshot` before executing any line. If any line fails and `flags.no_rollback` is false, it restores `ctx`, `config`, and `current_env` from the snapshot and calls `config.save()`. New script flags that change execution semantics must be coordinated with this rollback path.

### 5.6 `suggest()` vs. `maybe_suggest()`

`suggest()` returns `std::optional<std::string>` — use it inside `kinds/` factories to set `d.help`. `maybe_suggest()` writes directly to `std::cout` and is a legacy helper; prefer `suggest()` in new code.

---

## 6. Cross-module touch points

Files that must be updated **together** when making a change (derived from `#include` relationships in source):

| Change                         | Files that must be updated together                                                                                                                                                                                                                                  |
| ------------------------------ | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Add a new command              | `src/ui/repl/highlighter.hpp` (`valid_cmds`); `src/ui/repl/completions.hpp` (`all_commands()`); `src/ui/repl/hints.hpp` (`hint_for_keyword()`); `src/parser/command/command_parser_registry.cpp` (parser registration); `src/commands/registry.cpp` (handler wiring) |
| Add subcommand to `:config`    | `src/ui/repl/completions.hpp` (`config_subcommands()`; `detail::complete_config()`); `src/ui/repl/hints.hpp` (`hint_for_keyword(":config")`); `src/diagnostics/kinds/config_errors.hpp` (available list in `unknown_config_subcommand()`)                            |
| Add subcommand to `:env`       | `src/ui/repl/completions.hpp` (`env_subcommands()`; `detail::complete_env()`); `src/ui/repl/hints.hpp` (`hint_for_keyword(":env")`); `src/diagnostics/kinds/env_errors.hpp` (available list in `unknown_env_subcommand()`)                                           |
| Add subcommand to `:history`   | `src/ui/repl/hints.hpp` (`hint_for_keyword(":history")`); `src/diagnostics/kinds/history_errors.hpp` (available list in `unknown_history_subcommand()`)                                                                                                              |
| Add flag to `:simplify`        | `src/ui/repl/completions.hpp` (`detail::complete_simplify_flags()`); `src/ui/repl/hints.hpp` (`hint_for_keyword(":simplify")`); `src/parser/command/subparsers/` (flag parser)                                                                                       |
| Change `ansi::` color constant | `src/ui/color.hpp`; any UI test in `tests/ui/` that checks ANSI-stripped output (ANSI stripping is automatic, but color logic changes may affect plain text)                                                                                                         |
| Change history file path logic | `src/ui/repl/history.hpp` (`setup_history()`); `src/utils/path_utils.hpp` (`get_history_file_path()`); `src/commands/handlers/` history handler                                                                                                                      |
| Add new completion context     | `src/ui/repl/completions.hpp` (`setup_completions()` callback); `src/ui/repl/completions.hpp` (new `detail::complete_*` helper); optionally `src/ui/repl/hints.hpp`                                                                                                  |

---

## 7. Testing checklist

### Build
- [ ] `ninja -C build` produces zero errors and zero new warnings.
- [ ] `./build/bin/ast_tests` passes all existing unit tests.
- [ ] `cd build && ctest` passes all tests (unit + UI).

### Module-specific

- [ ] If a command was added or removed, its name (and any aliases) appear in **both** `valid_cmds` in `src/ui/repl/highlighter.hpp` **and** `all_commands()` in `src/ui/repl/completions.hpp`.
- [ ] The command is registered in `build_registry()` in `src/parser/command/command_parser_registry.cpp`.
- [ ] If a hint was added, `hint_for_keyword()` in `src/ui/repl/hints.hpp` maps the command string to its argument syntax.
- [ ] Any new ANSI color output calls `ansi::reset` after the styled region — confirmed by visual inspection in the REPL.
- [ ] Script behavior tested manually with `--script tests/ui/math_solve.msl` to confirm rollback, silent, and dry-run paths are unaffected.
- [ ] New `suggest()` usages call `suggest()` (not `maybe_suggest()`) when inside a `kinds/` factory; result is placed in `d.help`.
- [ ] Completion callback tested with Tab in the REPL for the new command or subcommand.

### Documentation
- [ ] `README.md` command table updated if user-facing command syntax changed.
- [ ] `docs/commands/<feature>.md` created or updated (run `/docs:command <feature>`).
- [ ] This module's contribution guide updated if the pattern checklist changed.
