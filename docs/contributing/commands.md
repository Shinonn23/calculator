# Contributing to Math Solver — `commands` Module

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

The `commands` module owns the full lifecycle of a colon-prefixed command: from raw string to AST node, through dispatch, to handler execution. It spans four directories: `src/ast/command/` (AST node types), `src/lexer/command/` (tokenizer), `src/parser/command/` (subparser registry and routing), and `src/commands/` (handler registry and per-command handler implementations).

The module does **not** own math expression parsing or evaluation (those live in `src/parser/math/` and `src/eval/`), nor does it own the algebra layer (`src/algebra/`). It delegates to those subsystems from within handler functions.

The REPL (`src/ui/repl/`) depends on this module for dispatch, history tracking, and command recognition in the highlighter. The `src/runtime/context/` and `src/config/` layers are owned externally but passed by reference into every handler.

```
Raw input string
  │
  ├─ CommandParser::parse()              src/parser/command/command_parser.cpp
  │    ├─ CommandTokenStream             src/lexer/command/command_token_stream.hpp
  │    └─ SubparserRegistry lookup       src/parser/command/command_parser_registry.cpp
  │         └─ ICommandSubparser::parse()  src/parser/command/subparsers/
  │              └─ XxxCommand (AST node)  src/ast/command/
  │
  └─ HandlerRegistry::dispatch(cmd)      src/commands/registry.hpp
       └─ cmd.accept(*this, sink)          double-dispatch via CommandVisitor
            └─ HandlerRegistry::visit(const XxxCommand&, DiagnosticSink&)
                 └─ handlers::handle_xxx()  src/commands/handlers/
```

---

## 2. Directory layout

```
src/ast/command/
├── command.hpp                  — Base Command class; owns raw_command_, source_file_, source_line_, accept() pure virtual
├── command_visitor.hpp          — Closed CommandVisitor; one pure-virtual visit() per concrete command type
├── system_command.hpp           — SystemCommand AST node; Type { Exit, Help, Clear, Ls, Unknown }
├── var_command.hpp              — VarCommand AST node; Action { Set, Unset, Unknown }; optional math_action_ and payload_
├── math_command.hpp             — MathCommand AST node; Type { Evaluate, Solve, Simplify, Expand, Factor, Unknown }; flags (isolated_, as_fraction_, specific_vars_)
├── env_command.hpp              — EnvCommand AST node; Action { Show, List, Load, Save, New, Delete, Move, Copy, Unknown }; Flags struct for --vars mode
├── config_command.hpp           — ConfigCommand AST node; Action { List, Get, Set, Path, Reset, Unknown }
├── history_command.hpp          — HistoryCommand AST node; Action { Show, ShowRange, Search, Save, Clear, Unknown }; limit, range, pattern, filepath, flags
├── load_command.hpp             — LoadCommand AST node; filepath and load flags (--dry-run, --silent, --env)
├── redo_command.hpp             — RedoCommand AST node; range selector (empty = last command)
└── history_entry.hpp            — HistoryEntry struct (command, status, timestamp); HistoryStatus enum with stable serialization strings

src/lexer/command/
├── command_token.hpp            — CommandTokenType enum and CommandToken struct with [start, end) byte offsets
├── command_lexer.hpp            — CommandLexer; single-pass tokenizer; pos_ <= input_.size() invariant
├── token_stream.hpp             — ITokenStream abstract interface; peek/advance/is_eof/raw_input/consume_remaining
├── command_token_stream.hpp     — CommandTokenStream implementing ITokenStream over CommandLexer; single-lookahead, no backtracking
└── command_token_stream.cpp     — CommandTokenStream constructor and advance() implementation

src/parser/command/
├── command_parser.hpp           — CommandParser and parse_command() free function; single-command entry point
├── command_parser.cpp           — Routing: empty→Evaluate, :cmd→SubparserRegistry, bare-word→SystemCommandParser, fallback→Evaluate
├── command_parser_registry.hpp  — SubparserRegistry type alias (unordered_map<string, unique_ptr<ICommandSubparser>>); build_registry() declaration
├── command_parser_registry.cpp  — build_registry() registering all subparsers with their trigger strings
└── subparsers/
    ├── icommand_subparser.hpp       — ICommandSubparser abstract interface; parse(ITokenStream&) → Result<CommandPtr>
    ├── system_command_parser.hpp    — Subparser for :exit/:quit/:q/:help/:h/:clear/:cls/:ls
    ├── var_command_parser.hpp       — Subparser for :set/:unset/:rm
    ├── math_command_parser.hpp      — Subparser for :solve/:simplify/:expand/:factor (flags: --vars, --isolated, --fraction)
    ├── env_command_parser.hpp       — Subparser for :env and its subcommands
    ├── config_command_parser.hpp    — Subparser for :config/:conf and its subcommands
    ├── history_command_parser.hpp   — Subparser for :history and its subcommands/selectors
    ├── load_command_parser.hpp      — Subparser for :load with flags (--dry-run, --silent, --env)
    └── redo_command_parser.hpp      — Subparser for :redo with optional range selector

src/commands/
├── registry.hpp                 — CommandRegistry<Cmd,Key> template; HandlerRegistry implementing CommandVisitor; build_handler_registry() declaration
├── registry.cpp                 — build_handler_registry() wiring all handlers; push_history(); all visit() implementations
└── handlers/
    ├── system_handler.hpp           — handle_system() for Exit/Help/Clear/Ls; print_help() listing all commands
    ├── var_handler.hpp              — handle_var(), handle_set(), handle_unset(); is_valid_identifier()
    ├── math_handler.hpp             — handle_math() dispatching to do_solve(), do_simplify(), do_expand(), do_factor(), do_evaluate()
    ├── env_handler.hpp              — handle_env() for all EnvCommand::Action variants; save_current_env(), load_env_into_context()
    ├── config_handler.hpp           — handle_config() for all ConfigCommand::Action variants
    ├── history_handler.hpp          — handle_history(); append_history_file(); load_history_file() with JSON Lines + legacy fallback
    ├── load_handler.hpp             — handle_load() delegating to Runner::run_script()
    └── redo_handler.hpp             — handle_redo<DispatchFn>() re-executing history entries by range
```

---

## 3. Core invariants

1. **CommandVisitor closed-set.** From `src/ast/command/command_visitor.hpp`: "All visit methods must be implemented; omitting a handler will result in a compile error. When introducing a new command type, CommandVisitor must be updated in lockstep to avoid silent dispatch failures. No default implementations are provided to force explicit handling of each command variant." Every class that inherits from `CommandVisitor` (currently only `HandlerRegistry`) must provide all eight `visit()` overloads.

2. **HandlerRegistry completeness.** From `src/commands/registry.hpp`: "All valid keys must be registered before any dispatch occurs." and "Failure to find a key during dispatch is fatal; this is relied upon to catch programming errors early." In `build_handler_registry()`, every enum variant listed for a command type must be registered; omitting one causes an abort-level diagnostic at runtime.

3. **SubparserRegistry key uniqueness.** From `src/parser/command/command_parser_registry.cpp`: "Each command string must be unique across all groups. Collisions result in silent overwrites, breaking dispatch correctness." Each trigger string (e.g. `:set`, `:solve`) must appear in exactly one `reg[...]` assignment.

4. **Highlighter sync.** From `src/ui/repl/highlighter.hpp`: "Invariant: `valid_cmds` must be kept in sync with the command parser. If a command is added/removed elsewhere, update here." The `valid_cmds` set in `is_valid_command()` is hardcoded and never auto-populated; it must be updated manually whenever `build_registry()` changes.

5. **Source location invariants.** From `src/ast/command/command.hpp`: "`source_file_` is always non-empty. `source_line_` is always >= 1." After construction, handlers may call `cmd.source_file()` / `cmd.source_line()` unconditionally; parsers must call `set_source()` before the command reaches any handler.

6. **CommandToken span integrity.** From `src/lexer/command/command_token.hpp`: "0 <= start <= end <= input.size(). The [start, end) range must correspond exactly to the token's span in the input. Consumers may rely on this for error reporting and diagnostics." Subparsers that manufacture synthetic tokens must set correct byte offsets for meaningful diagnostic highlighting.

7. **ICommandSubparser stream contract.** From `src/parser/command/subparsers/icommand_subparser.hpp`: "Implementations must either consume a valid command or leave the stream in a recoverable state for error handling. Partial consumption is discouraged unless explicitly coordinated with the parser driver."

8. **ITokenStream peek/advance consistency.** From `src/lexer/command/token_stream.hpp`: "peek() and advance() must always return a valid `CommandToken` unless `is_eof()` is true." and "`raw_input()` must return a reference to the original input buffer; lifetime must outlive the stream."

9. **HistoryStatus serialization stability.** From `src/ast/command/history_entry.hpp`: "The mapping must remain stable across versions for on-disk compatibility. Any change here must be coordinated with `parse_history_status`." The strings `"success"`, `"error"`, `"warning"`, `"info"`, `"unknown"` are the on-disk representation and must not be renamed.

10. **HistoryStatus mutual exclusivity.** From `src/ast/command/history_entry.hpp`: "Only one of `is_success()`, `is_error()`, `is_warning()`, `is_info()` is true at a time." All new `HistoryEntry` objects must be assigned exactly one `HistoryStatus` variant.

11. **No exceptions for user-facing errors.** Handlers return `HistoryStatus` and push `Diagnostic` objects to `DiagnosticSink`. They must not throw `std::runtime_error` or any other exception for user-visible failures. Exceptions are only acceptable for programming errors caught internally by the parser driver.

12. **Output via DiagnosticSink only.** Handlers must call `sink.push_output(text)` for human-readable results and `sink.push(diagnostic)` for errors. Direct writes to `std::cout` from inside a handler are forbidden.

---

## 4. Common contribution patterns

### 4.1 Adding a new top-level command

```
Trigger: User needs a new colon-prefixed command that does not fit any existing command type.
         Example: adding `:debug` or `:stats`.
```

1. **Create `src/ast/command/xxx_command.hpp`** — Subclass `Command`. Define a `Type` or `Action` enum for subcommands. Implement `accept()` calling `visitor.visit(*this, sink)`. Set `raw_command_` via the `Command(raw)` constructor.

2. **Update `src/ast/command/command_visitor.hpp`** — Add one pure-virtual overload:
   ```cpp
   // src/ast/command/command_visitor.hpp
   virtual void visit(const XxxCommand& cmd, DiagnosticSink& sink) = 0;
   ```

3. **Create `src/parser/command/subparsers/xxx_command_parser.hpp`** — Implement `ICommandSubparser`. Consume tokens from `ITokenStream&`; return `Result<CommandPtr>::ok(...)` on success or `Result<CommandPtr>::err(Diagnostic::make(...))` on failure. Satisfy the stream contract: do not leave the stream in a partially consumed state on success.

4. **Register in `src/parser/command/command_parser_registry.cpp`** — Inside `build_registry()`, add:
   ```cpp
   // src/parser/command/command_parser_registry.cpp
   reg[":xxx"] = std::make_unique<XxxCommandParser>();
   ```
   Verify the trigger string does not collide with any existing key.

5. **Create `src/commands/handlers/xxx_handler.hpp`** — Implement `handle_xxx(const XxxCommand& cmd, Context& ctx, Config& cfg, DiagnosticSink& sink) -> HistoryStatus`. Push output with `sink.push_output()` and errors with `sink.push(Diagnostic::make(...).with_location(cmd.source_file(), cmd.source_line()))`.

6. **Update `src/commands/registry.hpp`** — Add a `using XxxReg = CommandRegistry<XxxCommand, XxxCommand::Type>;` typedef, a private member `XxxReg xxx_reg_;`, a public `XxxReg& xxx() { return xxx_reg_; }` accessor, and declare `void visit(const XxxCommand& cmd, DiagnosticSink& sink) override;`.

7. **Update `src/commands/registry.cpp`** — Implement `HandlerRegistry::visit(const XxxCommand& cmd, DiagnosticSink& sink)` to dispatch through `xxx_reg_`. In `build_handler_registry()`, register all `XxxCommand::Type` variants:
   ```cpp
   // src/commands/registry.cpp
   for (auto type : {XxxCommand::Type::A, XxxCommand::Type::B}) {
       reg.xxx().add(type, [](const XxxCommand& cmd, Context& ctx,
                              Config& cfg, std::string& /*env*/,
                              DiagnosticSink& sink) -> HistoryStatus {
           return handlers::handle_xxx(cmd, ctx, cfg, sink);
       });
   }
   ```

8. **Update `src/ui/repl/highlighter.hpp`** — Add all trigger strings (e.g. `":xxx"`) to `valid_cmds`. This is a hardcoded set; there is no automatic synchronization.

9. **Update `src/commands/handlers/system_handler.hpp` (`print_help`)** — Add help text for the new command in the appropriate section so `:help` stays accurate.

10. **Build check** — `ninja -C build` must produce zero errors. The compiler will report any `CommandVisitor` implementation that is missing a `visit()` override.

---

### 4.2 Adding a subcommand to an existing command

```
Trigger: An existing command type needs a new action. Example: adding :env rename as an alias for :env mv,
         or adding :history export alongside :history save.
```

1. **Update the AST node** — Add the new variant to the `Action` (or `Type`) enum in `src/ast/command/xxx_command.hpp`. No other changes are needed to the `Command` base or `CommandVisitor`.

2. **Update the subparser** — Open `src/parser/command/subparsers/xxx_command_parser.hpp` (or `.cpp`) and add a branch recognising the new subcommand token.

3. **Update the handler** — Open `src/commands/handlers/xxx_handler.hpp` and add a `case XxxCommand::Action::NewVariant:` branch in the `switch` statement. Returning `HistoryStatus::Unknown` from an unhandled `case` is a detectable correctness bug.

4. **Register the new variant in `build_handler_registry()`** — In `src/commands/registry.cpp`, add `XxxCommand::Action::NewVariant` to the `for (auto action : {...})` initializer list so the handler closure is registered. Missing registration triggers an abort-level diagnostic on first dispatch.

5. **Update `src/commands/handlers/system_handler.hpp` (`print_help`)** — Add help text for the new subcommand.

6. **Build and test** — `ninja -C build` then `cd build && ctest`.

---

### 4.3 Adding a flag to an existing command

```
Trigger: A command needs a new optional flag, e.g. adding --verbose to :history show
         or --format to :config list.
```

1. **Extend the AST node** — Add a field to `src/ast/command/xxx_command.hpp`. For boolean flags follow the pattern of `MathCommand::isolated_` / `as_fraction_`; for string flags follow `EnvCommand::Flags::to_env`. Provide a setter (e.g. `set_flags()`) and a const accessor.

2. **Update the subparser** — In `src/parser/command/subparsers/xxx_command_parser.hpp`, consume `CommandTokenType::Flag` tokens and call the new setter on the AST node being constructed.

3. **Use the flag in the handler** — In `src/commands/handlers/xxx_handler.hpp`, read the flag via the const accessor and alter behavior accordingly.

4. **Update `print_help()`** — Add a `flag("--new-flag", "description")` line in `src/commands/handlers/system_handler.hpp`.

---

## 5. Patterns and conventions

### 5.1 Error reporting

Always return `Result<T>` from fallible parsing functions and push `Diagnostic` objects from handlers. Never throw for user-visible errors.

```cpp
// Good — subparser returning a parse error
// src/parser/command/subparsers/xxx_command_parser.hpp
auto t = stream.peek();
return Result<CommandPtr>::err(
    Diagnostic::make("expected argument after :xxx", "E0200",
                     Span{t.start, t.end}, stream.raw_input()));

// Good — handler surfacing an error with source location
// src/commands/handlers/xxx_handler.hpp
sink.push(Diagnostic::make("unknown subcommand", "E0500", span, raw)
              .with_location(cmd.source_file(), cmd.source_line()));
return HistoryStatus::Error;

// Bad — throws for a user-facing error
throw std::runtime_error("unknown subcommand");
```

Use the per-subsystem factory functions in `src/diagnostics/kinds/` (e.g. `errors::unknown_command()`, `errors::missing_var_name()`) when they exist for the error kind.

### 5.2 Visitor implementation

Every `CommandVisitor` implementation must cover all eight pure-virtual `visit()` overloads. Never add a default (no-op) implementation to `CommandVisitor` — that would allow silent dispatch failures when a new command type is introduced. The compiler enforces completeness via abstract-class errors.

```cpp
// src/commands/registry.hpp — must declare all eight overrides
void visit(const SystemCommand& cmd,  DiagnosticSink& sink) override;
void visit(const VarCommand& cmd,     DiagnosticSink& sink) override;
void visit(const MathCommand& cmd,    DiagnosticSink& sink) override;
void visit(const EnvCommand& cmd,     DiagnosticSink& sink) override;
void visit(const ConfigCommand& cmd,  DiagnosticSink& sink) override;
void visit(const LoadCommand& cmd,    DiagnosticSink& sink) override;
void visit(const HistoryCommand& cmd, DiagnosticSink& sink) override;
void visit(const RedoCommand& cmd,    DiagnosticSink& sink) override;
```

### 5.3 Source location propagation

Set source metadata on the command before it reaches any handler. Parsers must call `cmd->set_source(file, line)` for script-mode commands. Handlers must attach location to every `Diagnostic` via `.with_location(cmd.source_file(), cmd.source_line())` so script errors show the correct file and line number.

### 5.4 Output vs. diagnostics

Push human-readable results via `sink.push_output(text)`. Push `Diagnostic` objects only for errors and warnings. Never write directly to `std::cout` from inside a handler or subparser.

### 5.5 Handler return values

| `HistoryStatus` | When to use                                                                           |
| --------------- | ------------------------------------------------------------------------------------- |
| `Success`       | Command completed; context may have been mutated                                      |
| `Error`         | Command failed; a `Diagnostic` was pushed to sink                                     |
| `Warning`       | Command completed with non-fatal issues; at least one warning `Diagnostic` was pushed |
| `Info`          | Read-only output with no state change (e.g. `:help`, `:ls`)                           |
| `Unknown`       | Unreachable default; indicates a missing `case` in a switch                           |

Handlers must never return `Unknown` on a code path that can actually be reached.

### 5.6 CommandLexer token types

| `CommandTokenType` | Trigger                                  | Example         |
| ------------------ | ---------------------------------------- | --------------- |
| `Command`          | First char is `:`                        | `:solve`        |
| `Flag`             | Starts with `-` followed by alpha or `-` | `--vars`, `-f`  |
| `Word`             | Any other non-whitespace, non-quote run  | `x`, `3.14`     |
| `QuotedString`     | Surrounded by `"..."`                    | `"my file.msl"` |
| `Eof`              | End of input                             | —               |

Negative numbers are classified as `Word`, not `Flag`, because the lexer checks `peek()` for alpha or `-` before emitting a `Flag` token.

---

## 6. Cross-module touch points

The table below lists files that **must be updated together** when making the named change. Derived from actual `#include` relationships in source.

| Change                                                  | Files that must be updated together                                                                                                                                                                                                                                                                                                                                                  |
| ------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| Add a new command type                                  | `src/ast/command/xxx_command.hpp` · `src/ast/command/command_visitor.hpp` · `src/parser/command/subparsers/xxx_command_parser.hpp` · `src/parser/command/command_parser_registry.cpp` · `src/commands/registry.hpp` · `src/commands/registry.cpp` · `src/commands/handlers/xxx_handler.hpp` · `src/ui/repl/highlighter.hpp` · `src/commands/handlers/system_handler.hpp` (help text) |
| Add a trigger string alias (e.g. `:conf` for `:config`) | `src/parser/command/command_parser_registry.cpp` · `src/ui/repl/highlighter.hpp`                                                                                                                                                                                                                                                                                                     |
| Add a new `Action` variant to an existing command       | `src/ast/command/xxx_command.hpp` · `src/parser/command/subparsers/xxx_command_parser.hpp` · `src/commands/handlers/xxx_handler.hpp` · `src/commands/registry.cpp` (registration loop) · `src/commands/handlers/system_handler.hpp` (help text)                                                                                                                                      |
| Change `HistoryStatus` string representation            | `src/ast/command/history_entry.hpp` (`to_string`) · `src/ast/command/history_entry.hpp` (`parse_history_status`) — on-disk history files become incompatible                                                                                                                                                                                                                         |
| Change handler signature                                | `src/commands/registry.hpp` (using Handler = ...) · every `.add(key, [](...)` closure in `src/commands/registry.cpp`                                                                                                                                                                                                                                                                 |
| Add a new command flag                                  | `src/ast/command/xxx_command.hpp` · `src/parser/command/subparsers/xxx_command_parser.hpp` · `src/commands/handlers/xxx_handler.hpp` · `src/commands/handlers/system_handler.hpp` (help text)                                                                                                                                                                                        |
| Change command lexer token types                        | `src/lexer/command/command_token.hpp` · `src/lexer/command/command_lexer.hpp` · `src/lexer/command/token_stream.hpp` · `command_token_type_name()` in `command_token.hpp`                                                                                                                                                                                                            |

---

## 7. Testing checklist

### Build
- [ ] `ninja -C build` produces zero errors and zero new warnings.
- [ ] `./build/bin/ast_tests` passes all existing unit tests.
- [ ] `cd build && ctest` passes all tests (unit + UI).

### Module-specific

- [ ] Every new `CommandVisitor` implementation covers all pure-virtual `visit()` methods — confirmed by a clean build (no abstract-class errors).
- [ ] Every new command type has all its `Action`/`Type` enum variants registered in `build_handler_registry()` in `src/commands/registry.cpp`. Missing variants produce an abort-level diagnostic on first dispatch, not a compile error.
- [ ] Every new trigger string registered in `src/parser/command/command_parser_registry.cpp` also appears in `valid_cmds` inside `src/ui/repl/highlighter.hpp`. Omitting this causes the highlighter to mark the command red.
- [ ] Every new `HistoryStatus` string value in `to_string()` has a matching branch in `parse_history_status()` in `src/ast/command/history_entry.hpp`.
- [ ] Every handler that can return `HistoryStatus::Error` pushes at least one `Diagnostic` to `sink` before returning, so the user always sees why the command failed.
- [ ] Subparsers that partially consume the token stream on failure leave the stream in a state documented by the `ICommandSubparser` contract (either fully consumed or at the next recoverable position).
- [ ] New commands with subcommands have their help text added to `print_help()` in `src/commands/handlers/system_handler.hpp`.
- [ ] For `:load`-adjacent changes: `Runner*` is set on `HandlerRegistry` before any `LoadCommand` is dispatched (`HandlerRegistry::set_runner()`).

### UI tests
- [ ] A new `.msl` script in `tests/ui/` exercises the new command and its error paths.
- [ ] Expected output is generated with `python3 tests/run_ui_tests.py --binary ./build/bin/cmath-solver --tests-dir tests/ui --bless` and reviewed before committing.

### Documentation
- [ ] `README.md` command table updated if user-facing syntax changed.
- [ ] `docs/commands/<feature>.md` created or updated (run `/docs:command <feature>`).
- [ ] This contribution guide updated if the pattern checklist changed.
