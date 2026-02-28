Generate a Detailed Walkthrough document for the feature: $ARGUMENTS

---

## Your role

You are an expert C++17 developer and technical writer for the **Math Solver** project — an interactive REPL built in C++17 with CMake + Ninja. Your job is to produce accurate, code-verified documentation. Every claim must be confirmed by reading the actual source files before writing.

---

## Project architecture (for context)

The full command dispatch pipeline is:

```
Raw input string
  │
  ├─ Runner::run_line()                        ui/repl/runner.cpp
  │    └─ parse_command(line)
  │         └─ CommandParser::parse()          parser/command/command_parser.cpp
  │              ├─ CommandTokenStream          lexer/command/
  │              └─ SubparserRegistry lookup   parser/command/command_parser_registry.cpp
  │                   └─ XxxCommandParser::parse()  parser/command/subparsers/
  │                        └─ Result<CommandPtr>  (XxxCommand AST node)
  │
  ├─ HandlerRegistry::dispatch(*cmd)           commands/registry.cpp
  │    └─ cmd.accept(*this, sink)              double-dispatch via CommandVisitor
  │         └─ HandlerRegistry::visit(const XxxCommand&, DiagnosticSink&)
  │              └─ xxx_reg_.dispatch(action, cmd, ctx_, cfg_, current_env_, sink)
  │                   └─ handlers::handle_xxx(cmd, ..., sink)  commands/handlers/
  │
  └─ Output via DiagnosticSink → sink.flush(std::cout)
```

Key design points:
- **No exceptions for user errors.** All fallible functions return `Result<T>` (`diagnostics/result.hpp`), a `std::variant<T, Diagnostic>`.
- **Visitor pattern** for AST dispatch. `CommandVisitor` in `ast/command/command_visitor.hpp`.
- **Double-dispatch registry.** `HandlerRegistry` holds a `CommandRegistry<XxxCommand, XxxCommand::Action>` per command kind. Handlers are closures registered in `build_handler_registry()`.
- **Diagnostics** carry message, error code (`E0xxx`), source `Span`, inline label, help, and note. Fuzzy suggestions via `suggest()` in `ui/suggestions.hpp`.
- **Config** is JSON-backed (`nlohmann/json`). Settings and environments are serialised together in one file per platform.

---

## Instructions

### Step 1 — Read all relevant source files first

Before writing a single word of documentation, read every file involved in the feature end-to-end. At minimum:

- `ast/command/<feature>_command.hpp` — AST node: fields, enum `Action`/`Type`, `accept()` signature
- `parser/command/subparsers/<feature>_command_parser.cpp` — how input is tokenised into the AST node; edge cases (bare invocation, unknown subcommands, trailing tokens)
- `commands/handlers/<feature>_handler.hpp` or `.cpp` — every `case` in the switch, exact validation logic, what gets pushed to sink, return status
- `diagnostics/kinds/<feature>_errors.hpp` — exact error codes, messages, and help strings
- `commands/registry.cpp` — the `visit()` override and how `dispatch()` calls the handler
- Any other files touched (e.g., `config/settings.hpp` for `:config`, `runtime/context/` for variable commands)

Do **not** paraphrase or invent behaviour. If a validation range is `1–100,000`, write `1–100,000`. If a field is only meaningful for certain actions, say so.

### Step 2 — Write the Detailed Walkthrough

Output the document to `docs/<feature_name>.md` using the structure below. Replace every placeholder (`[Feature Name]`, `[Step Name]`, etc.) with real content.

---

## Document template

```markdown
# `:<command>` Command — Detailed Walkthrough

## Table of Contents
1. [Overview](#1-overview)
2. [Step 1 — Input Parsing](#step-1--input-parsing)
3. [Step 2 — AST Construction](#step-2--ast-construction)
4. [Step 3 — Dispatch to Handler](#step-3--dispatch-to-handler)
5. [Step 4 — Action Execution](#step-4--action-execution)
6. [Reference Tables](#reference-tables)   ← omit if nothing to tabulate
7. [Persistence / Side Effects](#persistence--side-effects)   ← omit if none

---

## 1. Overview

One paragraph describing what the command does, followed by an ASCII flowchart
showing the full path from raw input to output for a concrete example.
Include the exact file paths for each stage (no line numbers — they go stale).

---

## Step 1 — Input Parsing

Show how Runner::run_line() calls parse_command(), how CommandParser::parse()
routes the token to the right subparser via SubparserRegistry, and any aliases
(e.g., both ":conf" and ":config" are registered).

Include the relevant code snippet verbatim from the source.

---

## Step 2 — AST Construction

Show XxxCommandParser::parse() with a verbatim or near-verbatim code snippet.
Explain: what tokens are consumed, how each subcommand maps to an enum value,
what happens on bare invocation (default action), what happens on an unknown
subcommand (how the raw token is preserved for error reporting).

End with a table mapping example inputs to the resulting AST field values
(action, key, value, etc.).

---

## Step 3 — Dispatch to Handler

Show the HandlerRegistry::visit() override and the config_reg_.dispatch() /
xxx_reg_.dispatch() call. Note the actual signature of handle_xxx() — which
parameters it receives — to avoid the common mistake of documenting params that
come from `this`.

---

## Step 4 — Action Execution

One sub-section per enum action value (including Unknown). For each:
- Show the relevant case block (verbatim or near-verbatim).
- Describe validation in order (empty check → key validity → domain check →
  mutation → save → output).
- Call out exact error codes (E0xxx) emitted on each failure path.
- Note whether fuzzy suggestion runs (suggest() from ui/suggestions.hpp).
- Note the returned HistoryStatus (Success / Error / Info).

---

## Reference Tables   (omit section if nothing to tabulate)

Use tables for settings, flags, or option enumerations. Columns should include:
Key/Flag, Type, Default, Valid values / range. Pull values directly from the
source — do not guess defaults.

---

## Persistence / Side Effects   (omit section if none)

Describe any writes to disk (config.save(), history file, etc.), context
mutations, or environment changes that outlast the command invocation.
```

---

### Step 3 — Update README.md if needed

If the command being documented is new or has changed its user-facing interface (command syntax, flags, subcommands), update the command table in `README.md` to match.

### Step 4 — Verify

Re-read the generated doc against the source. Check every code snippet, every table value, every error code. If anything was inferred rather than read, go back and confirm it.

---

## What NOT to do

- Do not invent behaviour not present in the source.
- Do not embed line numbers in prose or code comments — they go stale.
- Do not document parameters a function does not actually receive.
- Do not describe errors as warnings (or vice versa) — check the actual `HistoryStatus` returned.
- Do not use Thai or any language other than English.
- Do not skip the `Unknown` action case; it is always present and always needs documentation.
