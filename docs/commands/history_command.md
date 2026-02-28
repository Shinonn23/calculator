# `:history` Command — Detailed Walkthrough

## Table of Contents

- [`:history` Command — Detailed Walkthrough](#history-command--detailed-walkthrough)
  - [Table of Contents](#table-of-contents)
  - [1. Overview](#1-overview)
  - [Step 1 — Input Parsing](#step-1--input-parsing)
  - [Step 2 — AST Construction](#step-2--ast-construction)
    - [Flag parsing](#flag-parsing)
    - [Subcommand dispatch](#subcommand-dispatch)
    - [`parse_search` and `parse_save`](#parse_search-and-parse_save)
    - [AST field values by input](#ast-field-values-by-input)
  - [Step 3 — Dispatch to Handler](#step-3--dispatch-to-handler)
  - [Step 4 — Action Execution](#step-4--action-execution)
    - [Show](#show)
    - [ShowRange](#showrange)
    - [Search](#search)
    - [Save](#save)
    - [Clear](#clear)
    - [Unknown](#unknown)
  - [Reference Tables](#reference-tables)
    - [Flags](#flags)
    - [Error codes](#error-codes)
    - [Config settings related to history](#config-settings-related-to-history)
  - [Persistence / Side Effects](#persistence--side-effects)
    - [Per-command recording](#per-command-recording)
    - [Startup loading](#startup-loading)
    - [History file location](#history-file-location)
    - [Clear side effects](#clear-side-effects)
    - [Save side effects](#save-side-effects)

---

## 1. Overview

`:history` manages the REPL's persistent command history. Each entry records
the command text, an execution status (`success`, `error`, `warning`, `info`),
and a millisecond-precision timestamp. The command supports five operations:
displaying a bounded tail or explicit range, substring search, saving to a
plain-text file, and clearing all history.

```
:history 10 --errors
  │
  ├─ Runner::run_line()                          src/ui/repl/runner.cpp
  │    └─ parse_command(":history 10 --errors")
  │         └─ CommandParser::parse()            src/parser/command/command_parser.cpp
  │              ├─ CommandTokenStream           src/lexer/command/
  │              └─ SubparserRegistry[":history"]
  │                   └─ HistoryCommandParser::parse()
  │                        src/parser/command/subparsers/history_command_parser.cpp
  │                        └─ HistoryCommand(Show, limit=10, flags=[Errors])
  │
  ├─ HandlerRegistry::dispatch(cmd)              src/commands/registry.cpp
  │    └─ cmd.accept(*this, sink)
  │         └─ HandlerRegistry::visit(const HistoryCommand&, sink)
  │              └─ handlers::handle_history(cmd, session_history_, sink)
  │                   src/commands/handlers/history_handler.hpp
  │
  └─ Output via DiagnosticSink → sink.flush(std::cout)
```

---

## Step 1 — Input Parsing

`Runner::run_interactive()` reads each line and calls `run_line()`.
`run_line()` calls the free function `parse_command()`, which constructs a
`CommandParser` and invokes its `parse()` method:

```cpp
// src/parser/command/command_parser.cpp
Result<CommandPtr> CommandParser::parse() {
    CommandTokenStream stream(raw_input_);
    // ...
    if (stream.peek_is(CommandTokenType::Command)) {
        std::string              cmd      = stream.peek().value;
        static SubparserRegistry registry = build_registry();
        auto                     it       = registry.find(cmd);
        if (it != registry.end()) {
            return it->second->parse(stream);
        }
        // ...
    }
    // ...
}
```

`build_registry()` maps `":history"` to a `HistoryCommandParser` instance
exactly once:

```cpp
// src/parser/command/command_parser_registry.cpp
reg[":history"] = std::make_unique<HistoryCommandParser>();
```

No alias is registered for `:history` (unlike `:config` / `:conf`).

---

## Step 2 — AST Construction

`HistoryCommandParser::parse()` always begins by consuming the `:history`
token, then parses any leading `--` flags before branching on the next token.

### Flag parsing

```cpp
// src/parser/command/subparsers/history_command_parser.cpp
stream.advance(); // consume ":history" token

std::vector<HistoryCommand::Flag> flags;
while (!stream.is_eof() && stream.peek().value.rfind("--", 0) == 0) {
    const std::string& f = stream.peek().value;
    if (f == "--errors")        flags.push_back(HistoryCommand::Flag::Errors);
    else if (f == "--success")  flags.push_back(HistoryCommand::Flag::Success);
    else if (f == "--info")     flags.push_back(HistoryCommand::Flag::Info);
    else if (f == "--warning")  flags.push_back(HistoryCommand::Flag::Warning);
    else                        flags.push_back(HistoryCommand::Flag::None);
    stream.advance();
}
```

Unknown flags are mapped to `Flag::None` for forward compatibility.
Flags may appear before or after the first positional argument (in the
`parse_show()` sub-path flags are re-parsed after the first numeric token).

### Subcommand dispatch

```cpp
if (stream.is_eof()) {
    // Default: show last 20 entries
    auto cmd = std::make_unique<HistoryCommand>(
        HistoryCommand::Action::Show, stream.raw_input());
    cmd->set_limit(20);
    cmd->set_flags(flags);
    return Result<CommandPtr>::ok(std::move(cmd));
}

const std::string& word = stream.peek().value;

if (word == "clear")  { stream.advance(); return /* Action::Clear */; }
if (word == "search") { stream.advance(); return parse_search(stream); }
if (word == "save")   { stream.advance(); return parse_save(stream);   }
if (word == "all")    { /* Action::Show, limit=0 */ }

// Range token: starts with digit and contains '-'
if (word.find('-') != std::string::npos &&
    std::isdigit(static_cast<unsigned char>(word[0]))) {
    auto range_r = HistoryRange::parse(range_str); // src/utils/history_range.hpp
    // → Action::ShowRange
}

// Single numeric token
if (std::isdigit(static_cast<unsigned char>(word[0]))) {
    int first = std::stoi(word);
    stream.advance();
    return parse_show(stream, first);
    // If second numeric token follows → Action::ShowRange([first..second])
    // Otherwise                       → Action::Show, limit=first
}

// Fallback (unrecognized token): Action::Show, limit=20
```

### `parse_search` and `parse_save`

**`parse_search`** collects all remaining input as the pattern with
`stream.consume_remaining()`. An empty result leaves `pattern_` unset;
the handler validates this at execution time.

**`parse_save`** reads the next token as `filepath_`, then calls
`stream.consume_remaining()`, strips spaces, and—if the selector starts with
a digit—passes it to `HistoryRange::parse()` to populate `range_`.

### AST field values by input

| Input                       | `action`  | `limit` | `pattern` | `filepath`  | `range`     |
| --------------------------- | --------- | ------- | --------- | ----------- | ----------- |
| `:history`                  | Show      | 20      | —         | —           | —           |
| `:history --errors`         | Show      | 20      | —         | —           | —           |
| `:history all`              | Show      | 0       | —         | —           | —           |
| `:history 10`               | Show      | 10      | —         | —           | —           |
| `:history 3 7`              | ShowRange | —       | —         | —           | [3,4,5,6,7] |
| `:history 1-5`              | ShowRange | —       | —         | —           | [1,2,3,4,5] |
| `:history 1-3,7-8`          | ShowRange | —       | —         | —           | [1,2,3,7,8] |
| `:history search foo`       | Search    | —       | `"foo"`   | —           | —           |
| `:history save out.txt`     | Save      | —       | —         | `"out.txt"` | —           |
| `:history save out.txt 1-3` | Save      | —       | —         | `"out.txt"` | [1,2,3]     |
| `:history clear`            | Clear     | —       | —         | —           | —           |

`range` values are sorted, deduplicated, and 1-based (produced by
`HistoryRange::parse()` in `src/utils/history_range.hpp`).

---

## Step 3 — Dispatch to Handler

`HandlerRegistry::dispatch()` calls `cmd.accept(*this, sink)`, which
double-dispatches to the `visit` override:

```cpp
// src/commands/registry.cpp
void HandlerRegistry::visit(const HistoryCommand& cmd, DiagnosticSink& sink) {
    // Clear triggers both deferred in-memory clear and replxx history clear.
    if (cmd.action() == HistoryCommand::Action::Clear) {
        should_clear_history_ = true;
        if (rx_)
            rx_->history_clear();
    }
    last_command_status_ =
        handlers::handle_history(cmd, session_history_, sink);
}
```

`handle_history` receives only the command node, the in-memory session history
vector, and the diagnostic sink — it does not receive `ctx_`, `cfg_`, or
`current_env_`. Registry state (`should_clear_history_`, `rx_`) is mutated
directly in `visit()` before delegating.

---

## Step 4 — Action Execution

All cases are handled inside `handlers::handle_history()` in
`src/commands/handlers/history_handler.hpp`.

### Show

```cpp
case HistoryCommand::Action::Show: {
    if (session_history.empty()) {
        sink.push_output("  No history\n");
        return HistoryStatus::Info;
    }
    int total = static_cast<int>(session_history.size());
    int start = (cmd.limit() == 0) ? 0 : std::max(0, total - cmd.limit());

    std::vector<std::pair<int, HistoryEntry>> entries;
    for (int i = start; i < total; ++i)
        if (should_show_entry(session_history[i], cmd))
            entries.emplace_back(i + 1, session_history[i]);

    print_history_entries(entries, sink);
    return HistoryStatus::Success;
}
```

- Empty history → pushes `"  No history\n"`, returns `Info`.
- `limit == 0` (`:history all`) → shows from index 0 (all entries).
- `limit > 0` → shows the last `limit` entries.
- Entries are filtered by `should_show_entry()`: if any flags are active, only
  entries whose status matches one of the active flags are included.
- `print_history_entries()` renders each entry as:
  `  [N] <timestamp> [ok|error|warn|info]  <command>` with ANSI color codes.
- Returns `Success`.

### ShowRange

```cpp
case HistoryCommand::Action::ShowRange: {
    std::vector<int> indices;
    std::string      err;
    if (!resolve_range(cmd.range(), extract_commands(session_history),
                       indices, err)) {
        sink.push(errors::history_range_error(
            raw, err, selector_span(raw), file, line));
        return HistoryStatus::Error;
    }
    std::vector<std::pair<int, HistoryEntry>> entries;
    for (int i : indices)
        if (should_show_entry(session_history[i], cmd))
            entries.emplace_back(i + 1, session_history[i]);

    print_history_entries(entries, sink);
    return HistoryStatus::Success;
}
```

- `resolve_range()` converts 1-based indices to 0-based, checking each against
  `[1, session_history.size()]`.
- Any out-of-range index → **E0701** (`history_range_error`), message:
  `"index N out of range (history has M entries)"`, returns `Error`.
- Valid range: filtered entries are printed. Returns `Success`.

### Search

```cpp
case HistoryCommand::Action::Search: {
    if (cmd.pattern().empty()) {
        sink.push(errors::history_missing_arg(
            raw, "search", "`:history search <pattern>`", file, line));
        return HistoryStatus::Error;
    }
    std::vector<std::pair<int, HistoryEntry>> matches;
    for (int i = 0; i < static_cast<int>(session_history.size()); ++i)
        if (session_history[i].command.find(cmd.pattern()) != std::string::npos
            && should_show_entry(session_history[i], cmd))
            matches.emplace_back(i + 1, session_history[i]);

    if (matches.empty()) {
        oss << "  No matches found for '" << cmd.pattern() << "'"
            << (cmd.has_any_flag() ? " with current filters" : "") << "\n";
        sink.push_output(oss.str());
        return HistoryStatus::Info;
    }
    print_history_entries(matches, sink);
    return HistoryStatus::Success;
}
```

- Empty pattern → **E0702** (`history_missing_arg`), help:
  `"Usage: \`:history search <pattern>\`"`, returns `Error`.
- Performs case-sensitive substring search across all entries, further filtered
  by active flags.
- No matches → pushes a `"No matches found"` message, returns `Info`.
- Matches found → prints filtered entries, returns `Success`.

### Save

```cpp
case HistoryCommand::Action::Save: {
    if (cmd.filepath().empty()) {
        sink.push(errors::history_missing_arg(
            raw, "save", "`:history save <file> [selector]`", file, line));
        return HistoryStatus::Error;
    }

    // ... collect entries (all, or by range) filtered by flags ...

    if (cmd.range().empty()) {
        // all entries, filtered by flags
    } else {
        if (!resolve_range(cmd.range(), cmds, indices, err)) {
            sink.push(errors::history_range_error(...));
            return HistoryStatus::Error;  // E0701
        }
    }

    if (!save_history_to_file(cmd.filepath(), entries)) {
        sink.push(errors::history_write_error(raw, cmd.filepath(), file, line));
        return HistoryStatus::Error;  // E0703
    }
    oss << "  Saved " << entries.size()
        << (cmd.has_any_flag() ? " filtered" : "")
        << " entry(s) to '" << cmd.filepath() << "'\n";
    sink.push_output(oss.str());
    return HistoryStatus::Success;
}
```

Validation order:
1. **Empty filepath** → **E0702** (`history_missing_arg`), help:
   `"Usage: \`:history save <file> [selector]\`"`, returns `Error`.
2. **Range validation** (if selector present) → **E0701** on out-of-range
   index, returns `Error`.
3. **File write failure** → **E0703** (`history_write_error`), message:
   `"cannot write to '<filepath>'"`, label:
   `"permission denied or path invalid"`, returns `Error`.
4. Success → prints `"  Saved N [filtered] entry(s) to '<file>'\n"`,
   returns `Success`.

`save_history_to_file()` writes one command string per line (plain text,
no timestamps or status tags).

### Clear

```cpp
case HistoryCommand::Action::Clear: {
    std::ofstream file(get_history_file_path(),
                       std::ios::trunc | std::ios::out);
    sink.push_output("  History cleared\n");
    return HistoryStatus::Info;
}
```

- Opens the history file with `ios::trunc`, which zeroes it.
- The replxx in-memory history is cleared by `rx_->history_clear()` in
  `visit()` (before `handle_history` is called).
- `should_clear_history_` is set in `visit()` for deferred clearing of
  `session_history_`; the in-memory vector is not cleared inside
  `handle_history` itself.
- Prints `"  History cleared\n"`, returns `Info`.

### Unknown

```cpp
case HistoryCommand::Action::Unknown: {
    const std::string& sub = cmd.raw_command();
    sink.push(errors::unknown_history_subcommand(raw, sub, file, line));
    return HistoryStatus::Error;
}
```

- Emits **E0002** with label `"unrecognized subcommand"`.
- `suggest()` (from `src/ui/suggestions.hpp`) checks the raw subcommand
  string against the known list `{"show", "search", "save", "clear"}`:
  - Match found → help: `"did you mean \`<suggestion>\`?"`.
  - No match → help: `"available: show, search, save, clear"`.
- Returns `Error`.

> **Note:** `HistoryCommandParser::parse()` never produces `Action::Unknown`.
> Unrecognized positional tokens fall through to the default `Action::Show`
> (limit=20) path. `Action::Unknown` is defined in the AST enum for
> completeness and forward compatibility, but the handler's `Unknown` case is
> currently unreachable via normal parsing.

---

## Reference Tables

### Flags

| Flag token               | `HistoryCommand::Flag` | Filters for              |
| ------------------------ | ---------------------- | ------------------------ |
| `--errors`               | `Errors`               | `HistoryStatus::Error`   |
| `--success`              | `Success`              | `HistoryStatus::Success` |
| `--warning`              | `Warning`              | `HistoryStatus::Warning` |
| `--info`                 | `Info`                 | `HistoryStatus::Info`    |
| _(any other `--` token)_ | `None`                 | (no filter applied)      |

Multiple flags may be combined; an entry passes the filter if it matches
**any** active flag (`OR` semantics). If no flags are given, all entries are
shown.

### Error codes

| Code  | Function                     | Trigger                                                         |
| ----- | ---------------------------- | --------------------------------------------------------------- |
| E0701 | `history_range_error`        | Index out of bounds in ShowRange or Save with selector          |
| E0702 | `history_missing_arg`        | Missing pattern for `search`, missing filepath for `save`       |
| E0703 | `history_write_error`        | Cannot open or write the output file in `save`                  |
| E0002 | `unknown_history_subcommand` | `Action::Unknown` (unreachable via parser; defined for safety)  |
| E0000 | `HistoryRange::parse`        | Malformed range syntax: index < 1, `lo > hi`, non-numeric token |

### Config settings related to history

| Key              | Type     | Default | Notes                                     |
| ---------------- | -------- | ------- | ----------------------------------------- |
| `history.size`   | `int`    | `1000`  | Maximum number of entries stored          |
| `history.dedup`  | `bool`   | `true`  | Deduplicate consecutive identical entries |
| `history.ignore` | `string` | `""`    | Pattern for entries to ignore             |

These settings are stored in `src/config/settings.hpp` and can be read or
changed with `:config get history.size` / `:config set history.size <n>`.

---

## Persistence / Side Effects

### Per-command recording

After every non-empty REPL line, `Runner::run_interactive()` calls
`HandlerRegistry::push_history(line, status)`:

```cpp
// src/commands/registry.cpp
void HandlerRegistry::push_history(const std::string& entry,
                                   HistoryStatus      status) {
    HistoryEntry new_entry;
    new_entry.command   = entry;
    new_entry.status    = status;
    new_entry.timestamp = get_current_timestamp(); // "YYYY-MM-DD HH:MM:SS.mmm"

    session_history_.emplace_back(new_entry);
    handlers::append_history_file(new_entry);   // appends to disk

    if (rx_ && !entry.empty())
        rx_->history_add(entry);                // replxx completion history
}
```

`append_history_file()` opens the history file in append mode and writes one
JSON Lines record:

```json
{"timestamp":"2026-02-28 10:00:00.123","status":"success","command":"2 + 3"}
```

### Startup loading

`build_handler_registry()` calls `load_history_file()` before registering any
handlers. This pre-populates `session_history_` from disk. The loader supports
two formats:

- **JSON Lines (current):** lines of the form `{"timestamp":...,"status":...,"command":...}`.
- **Legacy:** a `<timestamp> [<status>]` header line followed by the command on
  the next line.

Unknown status strings are silently parsed as `HistoryStatus::Unknown`.

### History file location

| Platform                      | Path                                       |
| ----------------------------- | ------------------------------------------ |
| Linux / macOS                 | `$HOME/.config/math-solver/history.txt`    |
| Windows                       | `%APPDATA%\math-solver\history.txt`        |
| Fallback (HOME/APPDATA unset) | `.math_solver_history` (current directory) |

Parent directories are created on first use by `get_history_file_path()` in
`src/utils/path_utils.hpp`.

### Clear side effects

`:history clear` affects three independent stores:

| Store                        | Mechanism                                                |
| ---------------------------- | -------------------------------------------------------- |
| On-disk history file         | Truncated via `ios::trunc` inside `handle_history`       |
| replxx completion history    | `rx_->history_clear()` called in `visit()`               |
| In-memory `session_history_` | `should_clear_history_` flag set in `visit()` (deferred) |

### Save side effects

`:history save <file>` writes plain text (command strings only, one per line)
to the user-specified path. It does not modify `session_history_`, the history
file, or replxx state.
