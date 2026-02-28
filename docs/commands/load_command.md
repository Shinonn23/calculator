# `:load` Command — Detailed Walkthrough

## Table of Contents
- [`:load` Command — Detailed Walkthrough](#load-command--detailed-walkthrough)
  - [Table of Contents](#table-of-contents)
  - [1. Overview](#1-overview)
  - [Step 1 — Input Parsing](#step-1--input-parsing)
  - [Step 2 — AST Construction](#step-2--ast-construction)
  - [Step 3 — Dispatch to Handler](#step-3--dispatch-to-handler)
  - [Step 4 — Script Execution](#step-4--script-execution)
    - [Phase 1 — File open](#phase-1--file-open)
    - [Phase 2 — Snapshot](#phase-2--snapshot)
    - [Phase 3 — Environment switch (`--env`)](#phase-3--environment-switch---env)
    - [Phase 4 — Line loop](#phase-4--line-loop)
    - [Phase 5 — Rollback and summary](#phase-5--rollback-and-summary)
    - [Summary output behaviour](#summary-output-behaviour)
  - [Reference Tables](#reference-tables)
    - [Flags](#flags)
    - [Error codes](#error-codes)
  - [Persistence / Side Effects](#persistence--side-effects)

---

## 1. Overview

`:load` reads a `.msl` script file and executes its lines one by one inside the
running solver session, sharing the same context, config, and environment.
Four behavioural flags (`--dry-run`, `--silent`, `--strict`, `--no-rollback`)
and an environment switch (`--env <name>`) let callers control what happens
before, during, and after execution. If any line produces an error the runner
records a rollback flag and, unless `--no-rollback` was given, restores the
pre-load snapshot of context, config, and current environment.

```
Input: ":load --strict setup.msl"
          │
          ▼
  Runner::run_line()                          src/ui/repl/runner.cpp
          │  parse_command(line)
          ▼
  CommandParser::parse()                      src/parser/command/command_parser.cpp
          │  SubparserRegistry lookup → ":load"
          ▼
  LoadCommandParser::parse()                  src/parser/command/subparsers/load_command_parser.cpp
          │  consumes tokens → LoadCommand{ filepath="setup.msl", flags={strict=true} }
          ▼
  HandlerRegistry::dispatch(*cmd)             src/commands/registry.cpp
          │  double-dispatch via CommandVisitor
          ▼
  HandlerRegistry::visit(const LoadCommand&)  src/commands/registry.cpp
          │  calls handle_load()
          ▼
  handlers::handle_load()                     src/commands/handlers/load_handler.hpp
          │  calls runner.run_script()
          ▼
  Runner::run_script()                        src/ui/repl/runner.cpp
          │  line-by-line: parse → dispatch → flush/rollback
          ▼
  Output via DiagnosticSink → sink.flush(std::cout)
```

---

## Step 1 — Input Parsing

`Runner::run_line()` calls `parse_command(line)`, which constructs a
`CommandParser` and invokes `CommandParser::parse()`. The parser reads the
first token to determine the command name, then looks it up in the
`SubparserRegistry` built by `build_registry()`.

`:load` is registered as a single entry with no aliases:

```cpp
// src/parser/command/command_parser_registry.cpp
// Load command: Not grouped; semantics are intentionally isolated.
reg[":load"] = std::make_unique<LoadCommandParser>();
```

Unlike `:config` (which also accepts `:conf`), `:load` has no short form.
Any other token is routed to a different subparser.

---

## Step 2 — AST Construction

`LoadCommandParser::parse()` is called with the token stream positioned at
the `:load` token. It consumes that token first, then reads flags and the
filepath in any order until EOF:

```cpp
// src/parser/command/subparsers/load_command_parser.cpp

Result<CommandPtr> LoadCommandParser::parse(ITokenStream& stream) {
    std::string raw = stream.raw_input();
    stream.advance(); // consume ":load" token

    if (stream.is_eof()) {
        Diagnostic err =
            errors::parse("missing filepath for ':load' command",
                          find_token_span(raw, ":load"), raw);
        err.help = "Usage: :load [flags] <filepath>";
        return Result<CommandPtr>::err(err);
    }

    LoadCommand::Flags flags;
    std::string        filepath;

    while (!stream.is_eof()) {
        const auto& token     = stream.peek();
        std::string token_val = token.value;

        if (token_val.rfind("--", 0) == 0) {
            stream.advance(); // consume flag

            if (token_val == "--dry-run") {
                flags.dry_run = true;
            } else if (token_val == "--silent") {
                flags.silent = true;
            } else if (token_val == "--strict") {
                flags.strict = true;
            } else if (token_val == "--no-rollback") {
                flags.no_rollback = true;
            } else if (token_val == "--env") {
                if (stream.is_eof()) {
                    Diagnostic err = errors::parse(
                        "expected environment name after '--env'",
                        find_token_span(raw, "--env"), raw);
                    err.help = "example: :load script.msl --env production";
                    return Result<CommandPtr>::err(err);
                }
                flags.env = stream.advance().value;
            } else {
                Diagnostic err =
                    errors::parse("unknown flag '" + token_val + "'",
                                  find_token_span(raw, token_val), raw);
                err.help = "available flags: --dry-run, --silent, "
                           "--strict, --no-rollback, --env";
                return Result<CommandPtr>::err(err);
            }
        } else {
            if (filepath.empty()) {
                filepath = stream.advance().value;
            } else {
                Diagnostic err = errors::parse(
                    "unexpected positional argument '" + token_val + "'",
                    find_token_span(raw, token_val), raw);
                err.inline_label = "extra argument";
                err.help = "only one filepath is supported per command.";
                return Result<CommandPtr>::err(err);
            }
        }
    }

    if (filepath.empty()) {
        Diagnostic err = errors::parse("no filepath provided",
                                       find_token_span(raw, ":load"), raw);
        err.help = "you must specify a file to load. Example: :load script.msl";
        return Result<CommandPtr>::err(err);
    }

    return Result<CommandPtr>::ok(
        std::make_unique<LoadCommand>(filepath, flags, raw));
}
```

**Token-consumption rules**

- The `:load` keyword is always consumed first.
- Any token starting with `--` is treated as a flag. Unknown `--` tokens are
  hard errors (E0001) to prevent silent misconfiguration.
- `--env` consumes the next token as the environment name. If no next token
  exists it is an immediate E0001.
- The first non-flag token becomes the filepath. A second non-flag token is
  an E0001 with `inline_label = "extra argument"`.
- Bare `:load` (nothing after the keyword) and `:load --flag1 --flag2` with
  no positional token both produce a "missing filepath" / "no filepath
  provided" E0001.

**Input → AST mapping**

| Input                                      | `filepath`    | `flags.dry_run` | `flags.silent` | `flags.strict` | `flags.no_rollback` | `flags.env` |
| ------------------------------------------ | ------------- | --------------- | -------------- | -------------- | ------------------- | ----------- |
| `:load setup.msl`                          | `"setup.msl"` | false           | false          | false          | false               | `""`        |
| `:load --dry-run setup.msl`                | `"setup.msl"` | true            | false          | false          | false               | `""`        |
| `:load setup.msl --silent --strict`        | `"setup.msl"` | false           | true           | true           | false               | `""`        |
| `:load --env prod setup.msl --no-rollback` | `"setup.msl"` | false           | false          | false          | true                | `"prod"`    |
| `:load`                                    | —             | —               | —              | —              | —                   | E0001       |
| `:load --badflg`                           | —             | —               | —              | —              | —                   | E0001       |
| `:load a.msl b.msl`                        | —             | —               | —              | —              | —                   | E0001       |

The resulting `LoadCommand` node stores `filepath_` and `flags_` as private
members and forwards `accept()` to the visitor pattern:

```cpp
// src/ast/command/load_command.hpp
void accept(CommandVisitor& visitor, DiagnosticSink& sink) const override {
    visitor.visit(*this, sink);
}
```

---

## Step 3 — Dispatch to Handler

`HandlerRegistry::dispatch()` calls `cmd.accept(*this, sink)`, which
double-dispatches to:

```cpp
// src/commands/registry.cpp
void HandlerRegistry::visit(const LoadCommand& cmd, DiagnosticSink& sink) {
    // runner_ must be set prior to handling LoadCommand.
    // If runner_ is unset, this is a fatal logic error.
    if (!runner_) {
        sink.push(Diagnostic::make(
            "HandlerRegistry: Runner not set; cannot handle :load", "E9999",
            Span{}));
        last_command_status_ = HistoryStatus::Error;
        return;
    }
    handlers::handle_load(cmd, *runner_, sink);
    last_command_status_ = HistoryStatus::Success;
}
```

Two things to note:

1. `runner_` is a pointer set by `Runner`'s constructor via
   `registry_.set_runner(*this)`. Its absence is a programming error, not a
   user error, hence error code `E9999`.
2. The return value of `handle_load()` is **not** used to set
   `last_command_status_`. The field is unconditionally set to
   `HistoryStatus::Success` after the call. Script-level errors are
   communicated through the diagnostic sink and the
   `runner.last_script_had_errors()` flag instead.

`handle_load` has the signature:

```cpp
// src/commands/handlers/load_handler.hpp
inline HistoryStatus handle_load(const LoadCommand& cmd, Runner& runner,
                                 DiagnosticSink& sink);
```

It receives the parsed command node, the runner, and the sink — it does **not**
receive `Context`, `Config`, or `current_env` directly; those are accessed
through `runner`.

---

## Step 4 — Script Execution

All logic lives in `Runner::run_script()` (`src/ui/repl/runner.cpp`). There
is no `Action` enum for `:load` — the single execution path is modulated by
the five `Flags` fields.

### Phase 1 — File open

```cpp
last_script_had_errors_ = false;
std::ifstream file(filepath);
if (!file.is_open()) {
    std::cout << ansi::red << "  Error: " << ansi::reset
              << "cannot open '" << filepath << "'\n";
    last_script_had_errors_ = true;
    return;
}
```

If the file cannot be opened the error is printed directly to `std::cout`
(not via the diagnostic sink). `last_script_had_errors_` is set to `true`
and the function returns immediately. `handle_load` then observes the flag
and pushes diagnostic E0900 "script '<filepath>' failed" with
`HistoryStatus::Error`.

### Phase 2 — Snapshot

```cpp
RuntimeSnapshot snap{registry_.ctx(), registry_.config(),
                     registry_.current_env()};
```

`RuntimeSnapshot` is a plain value struct containing copies of `Context`,
`Config`, and the current environment name. Capturing it here enables full
rollback if any subsequent line fails.

### Phase 3 — Environment switch (`--env`)

```cpp
if (!flags.env.empty()) {
    if (!registry_.config().env_exists(flags.env)) {
        std::cout << ansi::red << "  Error: " << ansi::reset
                  << "environment '" << flags.env << "' does not exist\n";
        last_script_had_errors_ = true;
        return;
    }
    old_env            = registry_.current_env();
    should_switch_back = true;
    run_line(":env load " + flags.env);
}
```

If `--env` was provided the runner first verifies the environment exists in
config. On failure it prints an error and returns (the snapshot is
implicitly abandoned — no rollback is needed because no mutation has
occurred yet). On success it switches by issuing an internal `:env load`
command through `run_line()`.

### Phase 4 — Line loop

Each line is processed as follows:

1. **Skip** empty lines and lines whose first non-whitespace character is `#`.
2. **Echo** the line (with its 1-based line number) to `std::cout` in dim
   ANSI style, unless `--silent` is set.
3. **Skip execution** if `--dry-run` is set (`continue` after echo).
4. **Parse** via `parse_command(trimmed)`. On failure:
   - Push the diagnostic to the local `DiagnosticSink` with source location.
   - Set `should_rollback = true`.
   - If `--strict`: break out of the loop immediately.
5. **Dispatch** `registry_.dispatch(*cmd, sink)`.
   - If not silent: flush output-class diagnostics to `std::cout`.
   - If silent: discard output-class diagnostics.
6. **Check status**: if `registry_.last_command_status() == HistoryStatus::Error`
   or `sink.has_errors()`, set `should_rollback = true`. If `--strict`: break.

### Phase 5 — Rollback and summary

```cpp
if (should_rollback) {
    last_script_had_errors_ = true;
}

if (should_rollback && !flags.no_rollback) {
    registry_.ctx()             = snap.ctx;
    registry_.config()          = snap.config;
    registry_.current_env_mut() = snap.current_env;

    registry_.config().save();

    sink.flush_summary(filepath, total);
    std::cout << ansi::yellow << ansi::dim
              << "  Rolled back — env and config unchanged\n"
              << ansi::reset;
    return;
}
```

If any line triggered an error and `--no-rollback` was **not** set, the
three mutable pieces of runtime state are restored from the snapshot
atomically, the config is persisted to disk, and the function returns after
printing the rollback notice.

If `--no-rollback` was set (or no error occurred), execution continues to
the env switch-back and summary output.

### Summary output behaviour

| Condition                  | Output                                                          |
| -------------------------- | --------------------------------------------------------------- |
| Errors present, not silent | `sink.flush()` then `"Loaded '<file>' (N line(s), E error(s))"` |
| No errors, not silent      | `sink.flush_summary(filepath, total)`                           |
| No errors, silent          | `sink.flush()` (warnings only, no summary line)                 |

---

## Reference Tables

### Flags

| Flag            | Field               | Default | Effect                                                      |
| --------------- | ------------------- | ------- | ----------------------------------------------------------- |
| `--dry-run`     | `flags.dry_run`     | `false` | Parse and echo lines; skip execution                        |
| `--silent`      | `flags.silent`      | `false` | Suppress echo and summary output                            |
| `--strict`      | `flags.strict`      | `false` | Stop on first parse or handler error                        |
| `--no-rollback` | `flags.no_rollback` | `false` | Preserve state even when errors occur                       |
| `--env <name>`  | `flags.env`         | `""`    | Switch to named environment before execution; restore after |

### Error codes

| Code    | Origin        | Message pattern                                          | `HistoryStatus`                 |
| ------- | ------------- | -------------------------------------------------------- | ------------------------------- |
| `E0001` | Parser        | `"missing filepath for ':load' command"`                 | — (parse fails before dispatch) |
| `E0001` | Parser        | `"unknown flag '<token>'"`                               | —                               |
| `E0001` | Parser        | `"expected environment name after '--env'"`              | —                               |
| `E0001` | Parser        | `"unexpected positional argument '<token>'"`             | —                               |
| `E0001` | Parser        | `"no filepath provided"`                                 | —                               |
| `E0900` | `handle_load` | `"script '<filepath>' failed"`                           | `Error`                         |
| `E9999` | `visit()`     | `"HandlerRegistry: Runner not set; cannot handle :load"` | `Error`                         |

---

## Persistence / Side Effects

**Rollback path (`should_rollback && !flags.no_rollback`)**

- `registry_.ctx()` is overwritten with the snapshot copy of `Context`.
- `registry_.config()` is overwritten with the snapshot copy of `Config`.
- `registry_.current_env_mut()` is reset to the snapshot's environment name.
- `registry_.config().save()` writes the restored config to disk
  (`~/.config/math-solver/` on Linux, `%APPDATA%\math-solver\` on Windows).

**Success path**

- Any variables, config changes, or environment mutations made by script
  lines survive in the live `Context` and `Config`. If a script line calls
  `:config set` or `:env save`, those handlers will have already written to
  disk as part of their normal operation.

**Environment switch (`--env`)**

- The environment switch performed internally by `run_line(":env load <name>")`
  goes through the normal `:env` handler and may write to disk.
- On rollback the environment is restored from the snapshot (and the config
  save covers the env switch too).
- On success with `--env`, the runner calls `run_line(":env load " + old_env)`
  at the end to restore the original environment.
