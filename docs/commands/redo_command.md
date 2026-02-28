# `:redo` Command — Detailed Walkthrough

## Table of Contents
1. [Overview](#1-overview)
2. [Step 1 — Input Parsing](#step-1--input-parsing)
3. [Step 2 — AST Construction](#step-2--ast-construction)
4. [Step 3 — Dispatch to Handler](#step-3--dispatch-to-handler)
5. [Step 4 — Action Execution](#step-4--action-execution)
6. [Reference Tables](#reference-tables)
7. [Persistence / Side Effects](#persistence--side-effects)

---

## 1. Overview

`:redo` re-executes one or more commands from the current session's history.
With no argument it re-runs the most recent command; with a numeric selector it
re-runs an explicit entry or a set of entries identified by index, list, or
range.  Each replayed command is parsed and dispatched through the normal
pipeline — output appears exactly as if the user had typed the command again —
and the replayed invocations are appended to history as new entries.

```
User types: :redo 3

Runner::run_line()                          src/ui/repl/runner.cpp
  └─ parse_command(":redo 3")
       └─ CommandParser::parse()            src/parser/command/command_parser.cpp
            ├─ CommandTokenStream lex        src/lexer/command/
            └─ SubparserRegistry[":redo"]   src/parser/command/command_parser_registry.cpp
                 └─ RedoCommandParser::parse()
                      ├─ advance past :redo  src/parser/command/subparsers/redo_command_parser.cpp
                      └─ HistoryRange::parse("3")  src/utils/history_range.hpp
                           └─ Result<CommandPtr>: RedoCommand{ range_=[3] }

HandlerRegistry::dispatch(*cmd)             src/commands/registry.hpp
  └─ cmd.accept(*this, sink)
       └─ HandlerRegistry::visit(const RedoCommand&, sink)  src/commands/registry.cpp
            └─ handlers::handle_redo(cmd, session_history_, lambda, sink)
                 ├─ resolve_range([3], commands, indices, err)  src/commands/handlers/history_handler.hpp
                 ├─ print dim echo of entry[2]
                 └─ dispatch_fn(entry[2])   → recursive parse + dispatch
```

---

## Step 1 — Input Parsing

`Runner::run_line()` calls `parse_command(line)` for every line of input
(`src/ui/repl/runner.cpp`):

```cpp
bool Runner::run_line(const std::string& line) {
    auto parse_result = parse_command(line);
    if (!parse_result) {
        registry_.sink().push(parse_result.error());
        registry_.sink().flush(std::cout);
        return true;
    }
    auto cmd    = std::move(*parse_result);
    bool status = registry_.dispatch(*cmd);
    registry_.sink().flush(std::cout);
    return status;
}
```

`parse_command()` in `src/parser/command/command_parser.cpp` constructs a
`CommandTokenStream` and checks the leading token.  When the first token is
a `CommandTokenType::Command` (i.e. it starts with `:`), the parser looks up
the token string in the static `SubparserRegistry`:

```cpp
if (stream.peek_is(CommandTokenType::Command)) {
    std::string              cmd      = stream.peek().value;
    static SubparserRegistry registry = build_registry();
    auto                     it       = registry.find(cmd);
    if (it != registry.end()) {
        try {
            return it->second->parse(stream);
        } catch (const std::exception& e) {
            auto t = stream.peek();
            return Result<CommandPtr>::err(Diagnostic::make(
                e.what(), "E0200", Span{t.start, t.end}, raw_input_));
        }
    }
    // Fallback: treat unknown colon-prefixed commands as SystemCommand::Unknown
    return Result<CommandPtr>::ok(std::make_unique<SystemCommand>(
        SystemCommand::Type::Unknown, raw_input_));
}
```

`build_registry()` in `src/parser/command/command_parser_registry.cpp`
registers exactly one entry for the redo command:

```cpp
reg[":redo"] = std::make_unique<RedoCommandParser>();
```

There are no aliases for `:redo`.

---

## Step 2 — AST Construction

`RedoCommandParser::parse()` in
`src/parser/command/subparsers/redo_command_parser.cpp` produces a
`RedoCommand` node:

```cpp
Result<CommandPtr> RedoCommandParser::parse(ITokenStream& stream) {
    stream.advance(); // always consume ":redo" token; parser invariant

    auto cmd = std::make_unique<RedoCommand>(stream.raw_input());

    // If no argument is present, default to redoing the last command.
    if (stream.is_eof())
        return Result<CommandPtr>::ok(std::move(cmd));

    const std::string& selector = stream.peek().value;

    // Only numeric selectors are accepted for redo ranges.
    if (selector.empty() ||
        !std::isdigit(static_cast<unsigned char>(selector[0])))
        return Result<CommandPtr>::ok(std::move(cmd));

    stream.advance();

    auto range_r = HistoryRange::parse(selector);
    if (range_r) {
        cmd->set_range(*range_r);
    } else {
        return Result<CommandPtr>::err(range_r.error());
    }

    return Result<CommandPtr>::ok(std::move(cmd));
}
```

Key parsing rules:

- The `:redo` token is always consumed unconditionally.
- If the stream is at EOF after the command token, `range_` is left empty
  (meaning "redo the last command").
- If the next token is present but its first character is not a digit (e.g. a
  flag like `--help` or an unknown subcommand), the parser silently ignores it
  and also leaves `range_` empty.  No error is emitted.
- If the next token begins with a digit it is treated as a selector string and
  passed to `HistoryRange::parse()`.  A `HistoryRange::parse()` failure
  propagates immediately as a `Result::err` (code `E0000`).

`HistoryRange::parse()` (in `src/utils/history_range.hpp`) converts the
selector string into a **sorted, deduplicated vector of 1-based indices**.
It accepts single indices, comma-separated lists, and inclusive dash-ranges in
any combination.  Malformed tokens, inverted ranges, or indices below 1 are
returned as `E0000` errors.  Upper-bound validation is deferred to the handler.

**Example inputs → AST fields:**

| Input            | `range_` (after parse)      | Notes                                  |
|------------------|-----------------------------|----------------------------------------|
| `:redo`          | `[]` (empty)                | Redo the most recent entry             |
| `:redo 5`        | `[5]`                       | Redo entry 5                           |
| `:redo 1,3,7`    | `[1, 3, 7]`                 | Redo entries 1, 3, and 7              |
| `:redo 4-6`      | `[4, 5, 6]`                 | Redo entries 4, 5, and 6              |
| `:redo 1,3,5-8`  | `[1, 3, 5, 6, 7, 8]`       | Mixed list + range, sorted/deduped    |
| `:redo --foo`    | `[]` (empty)                | Non-digit selector ignored silently   |
| `:redo 0`        | parse error (`E0000`)       | Index must be >= 1                    |
| `:redo 6-3`      | parse error (`E0000`)       | Inverted range rejected               |

The `RedoCommand` AST node (`src/ast/command/redo_command.hpp`) stores:

```cpp
class RedoCommand : public Command {
    std::vector<int> range_;   // empty = redo last; otherwise 1-based indices
public:
    RedoCommand(const std::string& raw) : Command(raw) {}
    void set_range(const std::vector<int>& range) { range_ = range; }
    const std::vector<int>& range() const { return range_; }
    void accept(CommandVisitor& visitor, DiagnosticSink& sink) const override {
        visitor.visit(*this, sink);
    }
};
```

---

## Step 3 — Dispatch to Handler

`HandlerRegistry::dispatch()` calls `cmd.accept(*this, sink)`, which
double-dispatches to the `visit` override for `RedoCommand` in
`src/commands/registry.cpp`:

```cpp
void HandlerRegistry::visit(const RedoCommand& cmd, DiagnosticSink& sink) {
    // RedoCommand replays a previous command by parsing and dispatching it.
    last_command_status_ = handlers::handle_redo(
        cmd, session_history_,
        [this, &sink](const std::string& raw) {
            auto parse_result = parse_command(raw);
            if (parse_result)
                dispatch(*std::move(*parse_result), sink);
        },
        sink);
}
```

Unlike most other command kinds, `RedoCommand` is **not** dispatched through a
`CommandRegistry<RedoCommand, Key>` lookup table.  The `redo_reg_` field exists
in `HandlerRegistry` but `build_handler_registry()` registers no entries in it.
The handler function is called directly from `visit()`.

`handle_redo()` receives:

| Parameter         | Type                                  | Source                        |
|-------------------|---------------------------------------|-------------------------------|
| `cmd`             | `const RedoCommand&`                  | The parsed AST node           |
| `session_history` | `const std::vector<HistoryEntry>&`    | `HandlerRegistry::session_history_` |
| `dispatch_fn`     | `DispatchFn` (templated callable)     | Lambda capturing `this`       |
| `sink`            | `DiagnosticSink&`                     | Forwarded from `visit()`      |

---

## Step 4 — Action Execution

`handle_redo()` is a function template in
`src/commands/handlers/redo_handler.hpp`.  It has two logical paths: empty
history and normal execution.

### Empty history

```cpp
if (session_history.empty()) {
    sink.push_output("  No history to redo\n");
    return HistoryStatus::Info;
}
```

If `session_history` contains no entries the handler outputs
`"  No history to redo\n"` and returns `HistoryStatus::Info`.  No diagnostic
is emitted; this is treated as an informational message, not an error.

### Normal execution

```cpp
std::vector<int>         indices;
std::string              err;

std::vector<std::string> commands;
for (const auto& entry : session_history)
    commands.push_back(entry.command);

if (cmd.range().empty()) {
    indices.push_back(static_cast<int>(session_history.size()) - 1);
} else {
    if (!resolve_range(cmd.range(), commands, indices, err)) {
        Diagnostic d =
            errors::math(
                err,
                Span(raw.find_last_of(" \t") + 1, raw.length()),
                raw)
                .with_label("invalid range");
        d.code = "E0801";
        sink.push(d);
        return HistoryStatus::Error;
    }
}

bool all_success = true;
for (int i : indices) {
    const std::string& entry = session_history[i].command;
    sink.push_output(std::string(ansi::dim) + "  >> " + entry +
                     ansi::reset + "\n");
    dispatch_fn(entry);
}
return all_success ? HistoryStatus::Success : HistoryStatus::Error;
```

**Index resolution:**

- If `cmd.range()` is empty, the 0-based index of the last history entry
  (`session_history.size() - 1`) is used.
- If `cmd.range()` is non-empty, `resolve_range()` converts the 1-based
  indices from `HistoryRange::parse()` into 0-based indices and validates that
  every index is within `[1, history.size()]`.  Out-of-bounds indices cause
  `resolve_range()` to return `false` and populate `err` with a message such as
  `"index 9 out of range (history has 5 entries)"`.

**Error path — invalid range (E0801):**

When `resolve_range()` returns `false`, a diagnostic is constructed via
`errors::math()` (initial code `E0000`) and then its `code` field is
overwritten to `"E0801"`.  The span covers the selector substring (from the
last whitespace in the raw command to its end), and the label is
`"invalid range"`.  The handler returns `HistoryStatus::Error`.

**Execution path — success:**

For each resolved 0-based index `i`:
1. The original command string `session_history[i].command` is echoed to the
   sink in dim ANSI styling: `  >> <command>`.
2. `dispatch_fn(entry)` is invoked.  The lambda calls `parse_command()` on the
   raw command string, and if parsing succeeds, calls `HandlerRegistry::dispatch()`
   — the same pipeline as an ordinary user input.

There is no mechanism for `dispatch_fn` to signal per-command failure back to
`handle_redo()`.  The local variable `all_success` is initialised to `true` and
is never set to `false`, so the function always returns `HistoryStatus::Success`
when it reaches the replay loop.

---

## Reference Tables

### Selector syntax

| Format       | Example      | Effect                              |
|--------------|--------------|-------------------------------------|
| Single index | `5`          | Replay entry 5                      |
| Comma list   | `1,3,7`      | Replay entries 1, 3, and 7          |
| Dash range   | `4-6`        | Replay entries 4, 5, and 6          |
| Mixed        | `1,3,5-8`    | Replay entries 1, 3, 5, 6, 7, 8    |
| (none)       |              | Replay the most recent entry        |

All indices are **1-based** in the user-facing selector; `resolve_range()`
converts them to 0-based before indexing `session_history`.

### Diagnostic codes

| Code    | Condition                                      | Returned status      |
|---------|------------------------------------------------|----------------------|
| `E0000` | Malformed selector string (parse phase)        | Parse error (propagated before dispatch) |
| `E0801` | Selector index out of bounds (handler phase)   | `HistoryStatus::Error` |

---

## Persistence / Side Effects

**Each replayed command is added to history as a new entry.**  After
`HandlerRegistry::visit()` returns, the main REPL loop calls
`registry_.push_history(line, status)` for the original `:redo` invocation.
Each individual command executed by `dispatch_fn` passes through
`HandlerRegistry::dispatch()`, which in turn calls the appropriate `visit()`
override; those handlers update `last_command_status_`, and the main loop
records each re-dispatched command's result in history via `push_history()`.

`push_history()` both appends to `session_history_` in memory and writes a
JSON Lines record to the on-disk history file (managed by
`handlers::append_history_file()` in `src/commands/handlers/history_handler.hpp`).

There are no other side effects: `:redo` does not modify `Context`, `Config`,
or any environment state directly — only the side effects of the replayed
commands themselves apply.
