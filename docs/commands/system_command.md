# System Commands — Detailed Walkthrough

## Table of Contents
- [System Commands — Detailed Walkthrough](#system-commands--detailed-walkthrough)
  - [Table of Contents](#table-of-contents)
  - [1. Overview](#1-overview)
  - [Step 1 — Input Parsing](#step-1--input-parsing)
    - [1.1 Colon-Prefixed Routing](#11-colon-prefixed-routing)
    - [1.2 Legacy Bare-Keyword Routing](#12-legacy-bare-keyword-routing)
    - [1.3 Unrecognised Colon-Prefix Fallback](#13-unrecognised-colon-prefix-fallback)
  - [Step 2 — AST Construction](#step-2--ast-construction)
  - [Step 3 — Dispatch to Handler](#step-3--dispatch-to-handler)
  - [Step 4 — Action Execution](#step-4--action-execution)
    - [Exit](#exit)
    - [Help](#help)
    - [Clear](#clear)
    - [Ls](#ls)
    - [Unknown](#unknown)
  - [Reference Tables](#reference-tables)
    - [Aliases](#aliases)
    - [`HistoryStatus` per action](#historystatus-per-action)
  - [Persistence / Side Effects](#persistence--side-effects)

---

## 1. Overview

System commands are REPL meta-commands that control the session itself rather
than performing any mathematical computation. They cover four operations:
terminating the session (`Exit`), displaying the help menu (`Help`), clearing
the terminal screen (`Clear`), and listing the variables currently in scope
(`Ls`). An `Unknown` type is also defined to carry any colon-prefixed token
that was not recognised by the subparser registry.

```
Input: ":help"
  │
  ├─ Runner::run_line()                     (src/ui/repl/runner.cpp)
  │    └─ parse_command(line)
  │         └─ CommandParser::parse()       (src/parser/command/command_parser.cpp)
  │              └─ SubparserRegistry lookup → SystemCommandParser::parse()
  │                   └─ Result<CommandPtr>  (SystemCommand, Type::Help)
  │
  ├─ HandlerRegistry::dispatch(*cmd)        (src/commands/registry.cpp)
  │    └─ cmd.accept(*this, sink)           (double-dispatch via CommandVisitor)
  │         └─ HandlerRegistry::visit(const SystemCommand&, DiagnosticSink&)
  │              └─ handlers::handle_system(cmd, ctx_, cfg_, should_exit_, sink)
  │
  └─ Output via DiagnosticSink → sink.flush(std::cout)
```

---

## Step 1 — Input Parsing

### 1.1 Colon-Prefixed Routing

`CommandParser::parse()` (`src/parser/command/command_parser.cpp`) first checks
whether the leading token is colon-prefixed. If it is, it looks up the token
in a static `SubparserRegistry` built once by `build_registry()`
(`src/parser/command/command_parser_registry.cpp`).

The following system command tokens are registered in that registry, all mapped
to `SystemCommandParser`:

```cpp
// src/parser/command/command_parser_registry.cpp
static const std::unordered_set<std::string> sys_cmds = {
    ":exit", ":quit", ":q", ":help", ":h", ":clear", ":cls", ":ls"};
for (const auto& cmd : sys_cmds)
    reg[cmd] = std::make_unique<SystemCommandParser>();
```

When found, the registry delegates to `SystemCommandParser::parse()` (see
[Step 2](#step-2--ast-construction)).

### 1.2 Legacy Bare-Keyword Routing

If the leading token is **not** colon-prefixed (e.g., `exit`, `quit`, `help`,
`clear`), `CommandParser::parse()` falls through to a second branch that tries
a static `SystemCommandParser` directly:

```cpp
// src/parser/command/command_parser.cpp
{
    std::string                cmd = stream.peek().value;
    static SystemCommandParser sys_parser;
    try {
        auto sys = sys_parser.parse(stream);
        if (sys && *sys != nullptr)
            return Result<CommandPtr>::ok(std::move(*sys));
    } catch (const std::exception& e) {
        auto t = stream.peek();
        return Result<CommandPtr>::err(Diagnostic::make(
            e.what(), "E0200", Span{t.start, t.end}, raw_input_));
    }
}
```

If `SystemCommandParser::parse()` returns `nullptr` (unrecognised bare word),
parsing falls through to treat the input as a math expression.

### 1.3 Unrecognised Colon-Prefix Fallback

If a colon-prefixed token is **not** found in the `SubparserRegistry` at all,
`CommandParser::parse()` creates a `SystemCommand` with `Type::Unknown`
directly—bypassing `SystemCommandParser` entirely:

```cpp
// src/parser/command/command_parser.cpp
return Result<CommandPtr>::ok(std::make_unique<SystemCommand>(
    SystemCommand::Type::Unknown, raw_input_));
```

This ensures that unknown colon commands like `:foo` are surfaced as errors
rather than silently treated as math expressions.

---

## Step 2 — AST Construction

`SystemCommandParser::parse()` (`src/parser/command/subparsers/system_command_parser.cpp`)
inspects the first token with `stream.peek().value` and performs exact-string
matching — no fuzzy or prefix matching is performed:

```cpp
// src/parser/command/subparsers/system_command_parser.cpp
Result<CommandPtr> SystemCommandParser::parse(ITokenStream& stream) {
    std::string cmd = stream.peek().value;
    if (cmd == ":exit" || cmd == ":quit" || cmd == ":q" || cmd == "exit" ||
        cmd == "quit" || cmd == "q") {
        return Result<CommandPtr>::ok(std::make_unique<SystemCommand>(
            SystemCommand::Type::Exit, stream.raw_input()));
    }
    if (cmd == ":help" || cmd == ":h" || cmd == "help" || cmd == "h") {
        return Result<CommandPtr>::ok(std::make_unique<SystemCommand>(
            SystemCommand::Type::Help, stream.raw_input()));
    }
    if (cmd == ":clear" || cmd == ":cls" || cmd == "clear" ||
        cmd == "cls") {
        return Result<CommandPtr>::ok(std::make_unique<SystemCommand>(
            SystemCommand::Type::Clear, stream.raw_input()));
    }
    if (cmd == ":ls") {
        return Result<CommandPtr>::ok(std::make_unique<SystemCommand>(
            SystemCommand::Type::Ls, stream.raw_input()));
    }
    return Result<CommandPtr>::ok(nullptr);
}
```

The raw input string is always forwarded to the AST node for diagnostic
round-tripping. `nullptr` is returned for any token that does not match — this
signals to the caller to fall through to the next parsing stage.

The resulting `SystemCommand` node (`src/ast/command/system_command.hpp`)
carries two fields: `type_` (a `Type` enum value) and `raw` (the original
input, inherited from `Command`).

**Input-to-AST mapping:**

| Input                                            | `Type`                                                        |
| ------------------------------------------------ | ------------------------------------------------------------- |
| `:exit` / `:quit` / `:q` / `exit` / `quit` / `q` | `Exit`                                                        |
| `:help` / `:h` / `help` / `h`                    | `Help`                                                        |
| `:clear` / `:cls` / `clear` / `cls`              | `Clear`                                                       |
| `:ls`                                            | `Ls`                                                          |
| Any unrecognised colon-prefix (e.g. `:foo`)      | `Unknown` (set by `CommandParser`, not `SystemCommandParser`) |

---

## Step 3 — Dispatch to Handler

`SystemCommand::accept()` calls `visitor.visit(*this, sink)`, which resolves to
`HandlerRegistry::visit(const SystemCommand&, DiagnosticSink&)` in
`src/commands/registry.cpp`:

```cpp
// src/commands/registry.cpp
void HandlerRegistry::visit(const SystemCommand& cmd,
                            DiagnosticSink&      sink) {
    // System commands may request process exit via should_exit_.
    // last_command_status_ must always reflect the result of handler.
    last_command_status_ =
        handlers::handle_system(cmd, ctx_, cfg_, should_exit_, sink);
}
```

Unlike other command kinds, system commands do **not** go through an action
sub-registry (`CommandRegistry<T, Action>`). The handler is called directly.

`handlers::handle_system()` (`src/commands/handlers/system_handler.hpp`)
receives:

| Parameter         | Type                   | Source                                                   |
| ----------------- | ---------------------- | -------------------------------------------------------- |
| `cmd`             | `const SystemCommand&` | the parsed AST node                                      |
| `ctx`             | `Context&`             | `HandlerRegistry::ctx_`                                  |
| `config`          | `Config&`              | `HandlerRegistry::cfg_` (unused, passed as `/*config*/`) |
| `out_should_exit` | `bool&`                | `HandlerRegistry::should_exit_`                          |
| `sink`            | `DiagnosticSink&`      | forwarded from `visit()`                                 |

---

## Step 4 — Action Execution

### Exit

```cpp
case SystemCommand::Type::Exit:
    out_should_exit = true;
    return HistoryStatus::Success;
```

Sets `out_should_exit` to `true`. The REPL runner observes this flag after
`dispatch()` returns and terminates the input loop. No resource cleanup is
performed inside the handler; that responsibility belongs to the higher-level
shutdown path. No output is pushed to `sink`.

Returns `HistoryStatus::Success`.

---

### Help

```cpp
case SystemCommand::Type::Help:
    print_help(sink);
    return HistoryStatus::Info;
```

Calls `print_help(sink)` (defined in the same header), which builds a
structured help menu covering all eight command sections via an `ostringstream`
and pushes the result as a single output string to `sink`. No state is mutated.

Returns `HistoryStatus::Info`.

---

### Clear

```cpp
case SystemCommand::Type::Clear:
    sink.push_output("\033[2J\033[H");
    return HistoryStatus::Info;
```

Pushes the ANSI escape sequence `\033[2J\033[H` (erase display + cursor home)
to `sink`. Behaviour depends on terminal support; no fallback is provided.

Returns `HistoryStatus::Info`.

---

### Ls

```cpp
case SystemCommand::Type::Ls: {
    if (ctx.empty()) {
        sink.push_output("  No variables defined\n");
        return HistoryStatus::Info;
    }
    size_t max_len = 0;
    for (const auto& [name, _] : ctx.all())
        max_len = std::max(max_len, name.size());

    std::ostringstream oss;
    for (const auto& [name, expr] : ctx.all()) {
        oss << "  " << name;
        for (size_t i = name.size(); i < max_len; ++i)
            oss << ' ';
        oss << "  =  " << expr->to_string() << "\n";
    }
    sink.push_output(oss.str());
    return HistoryStatus::Success;
}
```

Queries the active `Context` via `ctx.all()`, which returns a stable snapshot
of all bound variables.

- If the context is empty: emits `"  No variables defined\n"` and returns
  `HistoryStatus::Info`.
- Otherwise: iterates all entries, computes the longest variable name for
  column alignment, and emits an aligned table in the form `  name  =  expr`.
  Variable expressions are rendered via `expr->to_string()` (symbolic, not
  evaluated). Returns `HistoryStatus::Success`.

---

### Unknown

```cpp
case SystemCommand::Type::Unknown: {
    std::string input   = cmd.raw_command();
    std::string bad_cmd = input.substr(0, input.find(' '));
    Diagnostic  e       = errors::unknown_command(
        bad_cmd, find_token_span(input, bad_cmd), input);
    e = e.with_location(cmd.source_file(), cmd.source_line());
    sink.push(e);
    return HistoryStatus::Error;
}
```

Extracts the first whitespace-delimited token from the raw input as `bad_cmd`,
then constructs a `Diagnostic` via `errors::unknown_command()`
(`src/diagnostics/kinds/command_errors.hpp`):

- **Error code**: `E0002`
- **Message**: `"unknown command: <bad_cmd>"`
- **Inline label**: `"unrecognized command"`
- **Help text**: `"available commands: :help, :env, :config, :history, etc."`

The diagnostic is annotated with the source location
(`cmd.source_file()`, `cmd.source_line()`) before being pushed to `sink`.
No fuzzy suggestion (`suggest()`) is applied in this handler.

Returns `HistoryStatus::Error`.

---

## Reference Tables

### Aliases

| Input token(s)                         | Parsed `Type` | Notes                                             |
| -------------------------------------- | ------------- | ------------------------------------------------- |
| `:exit` `:quit` `:q` `exit` `quit` `q` | `Exit`        | Bare forms via legacy branch                      |
| `:help` `:h` `help` `h`                | `Help`        | Bare forms via legacy branch                      |
| `:clear` `:cls` `clear` `cls`          | `Clear`       | Bare forms via legacy branch                      |
| `:ls`                                  | `Ls`          | Colon form only; no bare alias                    |
| Any unrecognised `:xxx`                | `Unknown`     | Set by `CommandParser`, not `SystemCommandParser` |

### `HistoryStatus` per action

| `Type`    | Context condition     | `HistoryStatus` returned |
| --------- | --------------------- | ------------------------ |
| `Exit`    | —                     | `Success`                |
| `Help`    | —                     | `Info`                   |
| `Clear`   | —                     | `Info`                   |
| `Ls`      | Context is empty      | `Info`                   |
| `Ls`      | Context has variables | `Success`                |
| `Unknown` | —                     | `Error`                  |

---

## Persistence / Side Effects

- **`should_exit_` flag**: `Type::Exit` sets `HandlerRegistry::should_exit_`
  to `true`. The REPL runner (`src/ui/repl/runner.cpp`) reads this flag after
  `dispatch()` returns to terminate the session loop. No writes to disk occur
  in the handler itself.
- **Terminal state**: `Type::Clear` emits ANSI escape codes directly to the
  sink output stream, visually clearing the terminal. This is a side effect on
  the terminal emulator, not on any in-process state.
- **Context read-only access**: `Type::Ls` reads `ctx.all()` as a snapshot but
  does not mutate the context.
- **No config writes**: The `Config&` parameter is accepted but explicitly
  unused (marked `/*config*/` in the function signature). No settings are
  persisted by any system command.
