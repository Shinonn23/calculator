# `:env` Command — Detailed Walkthrough

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

The `:env` command manages **named environments** — named snapshots of the current variable context stored in the JSON config file. Environments let users switch between independent sets of variables without losing work. The command supports eight actions: displaying the active environment (`show`), listing all environments (`list`/`ls`), switching into one (`load`), persisting the current context (`save`), creating an empty slot (`new`), removing a slot (`delete`), renaming a slot (`mv`/`move`), and duplicating a slot (`cp`/`copy`). Both `mv` and `cp` also support a `--vars` mode that operates on a subset of variables rather than on a whole environment.

```
User types: ":env load work"
                │
                ▼
Runner::run_line()                          src/ui/repl/runner.cpp
  └─ parse_command(":env load work")
       └─ CommandParser::parse()            src/parser/command/command_parser.cpp
            ├─ CommandTokenStream           src/lexer/command/
            └─ SubparserRegistry[":env"]    src/parser/command/command_parser_registry.cpp
                 └─ EnvCommandParser::parse()
                      └─ Result<CommandPtr> src/parser/command/subparsers/env_command_parser.cpp
                           (EnvCommand{ action=Load, target_env="work" })
                │
                ▼
HandlerRegistry::dispatch(EnvCommand)       src/commands/registry.cpp
  └─ cmd.accept(*this, sink)
       └─ HandlerRegistry::visit(EnvCommand, DiagnosticSink)
            └─ env_reg_.dispatch(Load, cmd, ctx_, cfg_, current_env_, sink)
                 └─ handlers::handle_env(cmd, ctx, cfg, current_env, sink)
                      └─ case Load:         src/commands/handlers/env_handler.hpp
                           save_current_env(); load_env_into_context(); current_env = "work"
                │
                ▼
DiagnosticSink → sink.flush(std::cout)
  "  Switched to environment 'work'"
```

---

## Step 1 — Input Parsing

`Runner::run_line()` receives the raw input string, immediately calls `parse_command()`, and dispatches the resulting `CommandPtr` through the handler registry:

```cpp
// src/ui/repl/runner.cpp
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

`parse_command()` constructs a `CommandParser`, which creates a `CommandTokenStream` over the input and checks whether the first token is a colon-prefixed command:

```cpp
// src/parser/command/command_parser.cpp
if (stream.peek_is(CommandTokenType::Command)) {
    std::string              cmd      = stream.peek().value;
    static SubparserRegistry registry = build_registry();
    auto                     it       = registry.find(cmd);
    if (it != registry.end()) {
        return it->second->parse(stream);
    }
    // Fallback: treat unknown colon-commands as SystemCommand::Unknown
    return Result<CommandPtr>::ok(std::make_unique<SystemCommand>(
        SystemCommand::Type::Unknown, raw_input_));
}
```

The `SubparserRegistry` is built once in `build_registry()`. The `:env` command is registered under the single key `":env"` — no aliases:

```cpp
// src/parser/command/command_parser_registry.cpp
reg[":env"] = std::make_unique<EnvCommandParser>();
```

---

## Step 2 — AST Construction

`EnvCommandParser::parse()` consumes the `:env` prefix token, then reads the next token to determine the action. The main parse method delegates to two specialised helpers for the `Move`/`Copy` and `Save` actions; all other actions are handled inline.

```cpp
// src/parser/command/subparsers/env_command_parser.cpp
Result<CommandPtr> EnvCommandParser::parse(ITokenStream& stream) {
    stream.advance(); // consume ":env"

    if (stream.is_eof()) {
        return Result<CommandPtr>::ok(std::make_unique<EnvCommand>(
            EnvCommand::Action::Show, "", stream.raw_input()));
    }

    std::string action_str = stream.peek().value;
    stream.advance();

    EnvCommand::Action action = EnvCommand::Action::Show;
    if (action_str == "list" || action_str == "ls")
        action = EnvCommand::Action::List;
    else if (action_str == "load")
        action = EnvCommand::Action::Load;
    else if (action_str == "save")
        action = EnvCommand::Action::Save;
    else if (action_str == "new")
        action = EnvCommand::Action::New;
    else if (action_str == "delete")
        action = EnvCommand::Action::Delete;
    else if (action_str == "move" || action_str == "mv")
        action = EnvCommand::Action::Move;
    else if (action_str == "copy" || action_str == "cp")
        action = EnvCommand::Action::Copy;

    if (action == EnvCommand::Action::Move ||
        action == EnvCommand::Action::Copy) {
        return parse_move_copy(stream, action);
    }

    if (action == EnvCommand::Action::Save) {
        return parse_save(stream);
    }

    // For all other actions, at most one argument (target_env) is expected.
    std::string target_env;
    if (!stream.is_eof()) {
        target_env = stream.peek().value;
        stream.advance();
    }

    return Result<CommandPtr>::ok(std::make_unique<EnvCommand>(
        action, target_env, stream.raw_input()));
}
```

**Bare invocation** — `:env` with no further tokens: the stream is immediately EOF after consuming `:env`, so `Show` is returned with an empty `target_env`.

**Unrecognised subcommand** — the if-else chain has no `else` branch that sets `Unknown`; any token that does not match a known subcommand leaves `action` as `Show` (the initialiser). The `Unknown` action value therefore cannot be produced by the current parser via normal input. The handler's `Unknown` case exists as a defensive fallback for code paths that construct `EnvCommand` directly (e.g., tests or future callers).

### `parse_move_copy()`

```cpp
Result<CommandPtr>
EnvCommandParser::parse_move_copy(ITokenStream& stream, EnvCommand::Action action) {
    EnvCommand::Flags flags;
    std::string       source_env, target_env;

    if (!stream.is_eof() && stream.peek().value == "--vars") {
        stream.advance();
        flags.vars_mode = true;

        std::vector<std::string> vars;
        while (!stream.is_eof() && stream.peek().value != "--to") {
            vars.push_back(stream.peek().value);
            stream.advance();
        }
        if (!stream.is_eof() && stream.peek().value == "--to") {
            stream.advance();
            if (!stream.is_eof()) {
                flags.to_env = stream.peek().value;
                stream.advance();
            }
        }
        auto cmd = std::make_unique<EnvCommand>(action, flags.to_env,
                                                stream.raw_input());
        cmd->set_flags(flags);
        cmd->set_vars_to_save(vars);
        return Result<CommandPtr>::ok(std::move(cmd));
    }

    // env mode: two positional arguments
    if (!stream.is_eof()) { source_env = stream.peek().value; stream.advance(); }
    if (!stream.is_eof()) { target_env = stream.peek().value; stream.advance(); }

    auto cmd = std::make_unique<EnvCommand>(action, target_env, stream.raw_input());
    cmd->set_source_env(source_env);
    cmd->set_flags(flags); // vars_mode = false
    return Result<CommandPtr>::ok(std::move(cmd));
}
```

### `parse_save()`

```cpp
Result<CommandPtr> EnvCommandParser::parse_save(ITokenStream& stream) {
    std::string              target_env;
    std::vector<std::string> vars;

    // target_env is optional; must not start with "--"
    if (!stream.is_eof() && stream.peek().value.rfind("--", 0) != 0) {
        target_env = stream.peek().value;
        stream.advance();
    }

    if (!stream.is_eof() && stream.peek().value == "--vars") {
        stream.advance();
        while (!stream.is_eof()) {
            vars.push_back(stream.peek().value);
            stream.advance();
        }
    }

    auto cmd = std::make_unique<EnvCommand>(EnvCommand::Action::Save,
                                            target_env, stream.raw_input());
    if (!vars.empty())
        cmd->set_vars_to_save(vars);
    return Result<CommandPtr>::ok(std::move(cmd));
}
```

### Input-to-AST mapping

| Input                         | `action` | `source_env` | `target_env` | `vars_to_save` | `flags.vars_mode` | `flags.to_env` |
| ----------------------------- | -------- | ------------ | ------------ | -------------- | ----------------- | -------------- |
| `:env`                        | Show     | —            | `""`         | `[]`           | false             | `""`           |
| `:env show`                   | Show     | —            | `""`         | `[]`           | false             | `""`           |
| `:env list`                   | List     | —            | `""`         | `[]`           | false             | `""`           |
| `:env ls`                     | List     | —            | `""`         | `[]`           | false             | `""`           |
| `:env load work`              | Load     | —            | `"work"`     | `[]`           | false             | `""`           |
| `:env save`                   | Save     | —            | `""`         | `[]`           | false             | `""`           |
| `:env save work`              | Save     | —            | `"work"`     | `[]`           | false             | `""`           |
| `:env save work --vars x y`   | Save     | —            | `"work"`     | `["x","y"]`    | false             | `""`           |
| `:env save --vars x y`        | Save     | —            | `""`         | `["x","y"]`    | false             | `""`           |
| `:env new work`               | New      | —            | `"work"`     | `[]`           | false             | `""`           |
| `:env delete work`            | Delete   | —            | `"work"`     | `[]`           | false             | `""`           |
| `:env mv src dst`             | Move     | `"src"`      | `"dst"`      | `[]`           | false             | `""`           |
| `:env move src dst`           | Move     | `"src"`      | `"dst"`      | `[]`           | false             | `""`           |
| `:env mv --vars x y --to dst` | Move     | —            | `"dst"`      | `["x","y"]`    | true              | `"dst"`        |
| `:env cp src dst`             | Copy     | `"src"`      | `"dst"`      | `[]`           | false             | `""`           |
| `:env copy src dst`           | Copy     | `"src"`      | `"dst"`      | `[]`           | false             | `""`           |
| `:env badcmd`                 | Show     | —            | `""`         | `[]`           | false             | `""`           |

---

## Step 3 — Dispatch to Handler

`HandlerRegistry::visit(const EnvCommand&, DiagnosticSink&)` is the double-dispatch entry point. It forwards to the per-action registry:

```cpp
// src/commands/registry.cpp
void HandlerRegistry::visit(const EnvCommand& cmd, DiagnosticSink& sink) {
    last_command_status_ = env_reg_.dispatch(cmd.action(), cmd, ctx_, cfg_,
                                             current_env_, sink);
}
```

All nine `EnvCommand::Action` values are registered in `build_handler_registry()` with the same closure:

```cpp
// src/commands/registry.cpp
for (auto action : {EnvCommand::Action::Show, EnvCommand::Action::List,
                    EnvCommand::Action::Load, EnvCommand::Action::Save,
                    EnvCommand::Action::New,  EnvCommand::Action::Delete,
                    EnvCommand::Action::Move, EnvCommand::Action::Copy,
                    EnvCommand::Action::Unknown}) {
    reg.env().add(
        action,
        [](const EnvCommand& cmd, Context& ctx, Config& cfg,
           std::string& env, DiagnosticSink& sink) -> HistoryStatus {
            return handlers::handle_env(cmd, ctx, cfg, env, sink);
        });
}
```

`handle_env` receives `cmd`, `ctx` (the live variable context), `cfg` (the config/environment store), `env` (the `current_env` string by reference, mutable), and `sink`.

---

## Step 4 — Action Execution

### 4.1 Show

```cpp
case EnvCommand::Action::Show: {
    std::ostringstream oss;
    oss << "  Current environment: " << ansi::bold << current_env
        << ansi::reset << "\n";
    sink.push_output(oss.str());
    return HistoryStatus::Info;
}
```

Prints the name of the active environment in bold. No validation required. Returns `HistoryStatus::Info`.

---

### 4.2 List

```cpp
case EnvCommand::Action::List: {
    auto envs = config.list_envs();
    if (envs.empty()) {
        sink.push_output("  No environments defined\n");
    } else {
        std::ostringstream oss;
        for (const auto& name : envs) {
            if (name == current_env)
                oss << "  * " << ansi::bold << name << ansi::reset
                    << " (current)\n";
            else
                oss << "    " << name << "\n";
        }
        sink.push_output(oss.str());
    }
    return HistoryStatus::Info;
}
```

Retrieves all environment names from `config.list_envs()`. The active environment is prefixed with `*` and printed in bold. Returns `HistoryStatus::Info`.

---

### 4.3 Load

```cpp
case EnvCommand::Action::Load: {
    const std::string& target = cmd.target_env();
    if (target.empty()) {
        sink.push(errors::missing_env_name(
            raw, "load", "`env load <name>`", file, line));
        return HistoryStatus::Error;
    }
    save_current_env(config, current_env, ctx);
    if (load_env_into_context(config, target, ctx, raw,
                              cmd.source_line(), cmd.source_file(), sink)) {
        current_env = target;
        // print confirmation ...
        return HistoryStatus::Success;
    }
    return HistoryStatus::Error;
}
```

Validation order:
1. **Empty target** → `E0600` (`missing_env_name`), `HistoryStatus::Error`.
2. **Save current env** — `save_current_env()` snapshots the live context into the config store before switching; no error is possible here.
3. **Load target** — `load_env_into_context()` calls `config.get_env(target)`. If the environment is not found, it emits `E0601` (`env_not_found`) which runs a fuzzy suggestion against `config.list_envs()` and returns `false` → `HistoryStatus::Error`.
4. **Update `current_env`** — the mutable reference is updated to `target`; this is immediately visible in the REPL prompt.
5. Returns `HistoryStatus::Success`.

---

### 4.4 Save

```cpp
case EnvCommand::Action::Save: {
    const std::string& target = cmd.target_env();
    const std::string& dest = target.empty() ? current_env : target;
    const auto&        vars_to_save = cmd.vars_to_save();
    bool               has_warning  = false;

    if (!vars_to_save.empty()) {
        std::unordered_map<std::string, std::string> subset;
        auto all = ctx.all_as_strings();
        for (const auto& v : vars_to_save) {
            if (auto it = all.find(v); it != all.end()) {
                subset[v] = it->second;
            } else {
                sink.push(errors::var_skipped_warning(raw, v, file, line));
                has_warning = true;
            }
        }
        config.save_env_variables(dest, subset);
    } else {
        save_current_env(config, dest, ctx);
    }
    // print confirmation ...
    return has_warning ? HistoryStatus::Warning : HistoryStatus::Success;
}
```

`dest` defaults to `current_env` when no explicit target is given.

With `--vars`: iterates `vars_to_save`. Each name not found in the live context emits a `Diagnostic::warning` (no error code, not `E0xxx`) with inline label `"not in current context"`. The loop continues and the successfully found subset is written. Returns `HistoryStatus::Warning` if any variable was skipped, `HistoryStatus::Success` otherwise.

Without `--vars`: writes the full context via `save_current_env()`. Returns `HistoryStatus::Success`.

---

### 4.5 New

```cpp
case EnvCommand::Action::New: {
    const std::string& name = cmd.target_env();
    if (name.empty()) {
        sink.push(errors::missing_env_name(
            raw, "new", "`env new <name>`", file, line));
        return HistoryStatus::Error;
    }
    auto res = config.create_env(name);
    if (!res) {
        Diagnostic d = res.error().with_location(file, line);
        d.span       = find_token_span(raw, name);
        d.input      = raw;
        sink.push(d);
        return HistoryStatus::Error;
    }
    config.save();
    // print confirmation ...
    return HistoryStatus::Success;
}
```

Validation order:
1. **Empty name** → `E0600`, `HistoryStatus::Error`.
2. **`config.create_env(name)`** — propagates any error from the config layer (e.g., duplicate name) with a span highlighting the name token. `HistoryStatus::Error`.
3. **`config.save()`** — writes the updated config to disk.
4. Returns `HistoryStatus::Success`.

---

### 4.6 Delete

```cpp
case EnvCommand::Action::Delete: {
    const std::string& name = cmd.target_env();
    if (name.empty()) {
        sink.push(errors::missing_env_name(
            raw, "delete", "`env delete <name>`", file, line));
        return HistoryStatus::Error;
    }
    if (name == current_env) {
        Diagnostic d =
            Diagnostic::make("cannot delete the active environment",
                             "E0602", find_token_span(raw, name),
                             raw, "active environment")
                .with_location(file, line);
        d.help = "switch first with `:env load <name>`";
        sink.push(d);
        return HistoryStatus::Error;
    }
    auto res = config.delete_env(name);
    if (!res) {
        Diagnostic d = res.error().with_location(file, line);
        d.span       = find_token_span(raw, name);
        d.input      = raw;
        sink.push(d);
        return HistoryStatus::Error;
    }
    config.save();
    // print confirmation ...
    return HistoryStatus::Success;
}
```

Validation order:
1. **Empty name** → `E0600`, `HistoryStatus::Error`.
2. **Active environment guard** → `E0602` with help text `"switch first with ':env load <name>'"`. `HistoryStatus::Error`.
3. **`config.delete_env(name)`** — propagates config-layer error (e.g., name not found). `HistoryStatus::Error`.
4. **`config.save()`** — persists the change.
5. Returns `HistoryStatus::Success`.

---

### 4.7 Move

Move has two distinct modes selected by `flags.vars_mode`.

#### 4.7a `--vars` mode

```cpp
if (flags.vars_mode) {
    const std::string& dest = flags.to_env;
    if (dest.empty()) {
        sink.push(errors::missing_env_name(
            raw, "--to", "`env mv --vars x y --to <env>`", file, line));
        return HistoryStatus::Error;
    }
    if (!config.env_exists(dest)) {
        sink.push(errors::env_not_found(raw, dest, config, file, line));
        return HistoryStatus::Error;
    }
    auto all = ctx.all_as_strings();
    std::unordered_map<std::string, std::string> subset;
    bool has_warning = false;
    for (const auto& v : cmd.vars_to_save()) {
        if (auto it = all.find(v); it != all.end()) {
            subset[v] = it->second;
            ctx.unset(v);
        } else {
            sink.push(errors::var_skipped_warning(raw, v, file, line));
            has_warning = true;
        }
    }
    config.save_env_variables(dest, subset);
    save_current_env(config, current_env, ctx);
    // print confirmation (count of moved variables) ...
    return has_warning ? HistoryStatus::Warning : HistoryStatus::Success;
}
```

Validation order:
1. **Empty `--to` destination** → `E0600`, `HistoryStatus::Error`.
2. **Destination env does not exist** → `E0601` with fuzzy suggestion, `HistoryStatus::Error`.
3. **Variable loop** — found variables are removed from `ctx` via `ctx.unset(v)` and collected into `subset`. Missing variables emit a `Diagnostic::warning` (no `E0xxx` code) per variable. The loop continues.
4. **`config.save_env_variables(dest, subset)`** — writes the subset to the destination env.
5. **`save_current_env(config, current_env, ctx)`** — re-snapshots the current env after removals.
6. Returns `HistoryStatus::Warning` if any variable was skipped, `HistoryStatus::Success` otherwise.

#### 4.7b Env rename mode

```cpp
} else {
    const std::string& src  = cmd.source_env();
    const std::string& dest = cmd.target_env();
    if (src.empty() || dest.empty()) {
        sink.push(errors::missing_env_name(
            raw, "mv", "`env mv <src> <dst>`", file, line));
        return HistoryStatus::Error;
    }
    if (src == current_env) {
        Diagnostic d = Diagnostic::make(
                           "cannot move the active environment",
                           "E0602", find_token_span(raw, src),
                           raw, "active environment")
                           .with_location(file, line);
        d.help = "switch first with `:env load <name>`";
        sink.push(d);
        return HistoryStatus::Error;
    }
    if (!config.env_exists(src)) {
        sink.push(errors::env_not_found(raw, src, config, file, line));
        return HistoryStatus::Error;
    }
    config.rename_env(src, dest);
    // print confirmation ...
    return HistoryStatus::Success;
}
```

Validation order:
1. **Either name empty** → `E0600`, `HistoryStatus::Error`.
2. **Source is active env** → `E0602` with help text `"switch first with ':env load <name>'"`, `HistoryStatus::Error`.
3. **Source env not found** → `E0601` with fuzzy suggestion, `HistoryStatus::Error`.
4. **`config.rename_env(src, dest)`** — renames the environment in the config store.
5. Returns `HistoryStatus::Success`.

---

### 4.8 Copy

```cpp
case EnvCommand::Action::Copy: {
    const std::string& src  = cmd.source_env();
    const std::string& dest = cmd.target_env();
    if (src.empty() || dest.empty()) {
        sink.push(errors::missing_env_name(
            raw, "cp", "`env cp <src> <dst>`", file, line));
        return HistoryStatus::Error;
    }
    if (!config.env_exists(src)) {
        sink.push(errors::env_not_found(raw, src, config, file, line));
        return HistoryStatus::Error;
    }
    config.copy_env(src, dest);
    // print confirmation ...
    return HistoryStatus::Success;
}
```

Validation order:
1. **Either name empty** → `E0600`, `HistoryStatus::Error`.
2. **Source env not found** → `E0601` with fuzzy suggestion, `HistoryStatus::Error`.
3. **`config.copy_env(src, dest)`** — duplicates the source environment under the destination name.
4. Returns `HistoryStatus::Success`.

Unlike `Move`, `Copy` does not prevent copying the active environment.

---

### 4.9 Unknown

```cpp
case EnvCommand::Action::Unknown: {
    const std::string&                    sub  = cmd.raw_command();
    static const std::vector<std::string> subs = {
        "show", "list",   "load", "save",
        "new",  "delete", "mv",   "cp"};
    sink.push(
        errors::unknown_env_subcommand(raw, sub, subs, file, line));
    return HistoryStatus::Error;
}
```

Emits `E0002` (`unknown_env_subcommand`) with a fuzzy suggestion (`suggest()` from `src/ui/suggestions.hpp`) against the canonical subcommand list `{"show", "list", "load", "save", "new", "delete", "mv", "cp"}`. If no close match is found, the diagnostic lists all available subcommands. Returns `HistoryStatus::Error`.

> **Note:** The current `EnvCommandParser` does not produce `Action::Unknown` for unrecognised tokens — an unrecognised subcommand token is silently consumed and the action defaults to `Show`. The `Unknown` handler is a defensive fallback for code that constructs `EnvCommand` directly (e.g., unit tests or future callers).

---

## Reference Tables

### Subcommand aliases

| Input token(s)  | `EnvCommand::Action` |
| --------------- | -------------------- |
| *(bare `:env`)* | `Show`               |
| `show`          | `Show` (via default) |
| `list`, `ls`    | `List`               |
| `load`          | `Load`               |
| `save`          | `Save`               |
| `new`           | `New`                |
| `delete`        | `Delete`             |
| `move`, `mv`    | `Move`               |
| `copy`, `cp`    | `Copy`               |

### Error codes

| Code                 | Factory                            | Condition                                                  |
| -------------------- | ---------------------------------- | ---------------------------------------------------------- |
| `E0600`              | `errors::missing_env_name()`       | Required name or `--to` argument is absent                 |
| `E0601`              | `errors::env_not_found()`          | Named environment does not exist; fuzzy suggestion runs    |
| `E0602`              | `Diagnostic::make()` (inline)      | Attempt to delete or move the currently active environment |
| `E0002`              | `errors::unknown_env_subcommand()` | Unrecognised subcommand; fuzzy suggestion runs             |
| *(warning, no code)* | `errors::var_skipped_warning()`    | Variable named in `--vars` is not in the current context   |

### `HistoryStatus` per action

| Action               | Success path | Error path | Warning path             |
| -------------------- | ------------ | ---------- | ------------------------ |
| Show                 | `Info`       | —          | —                        |
| List                 | `Info`       | —          | —                        |
| Load                 | `Success`    | `Error`    | —                        |
| Save                 | `Success`    | —          | `Warning` (skipped vars) |
| New                  | `Success`    | `Error`    | —                        |
| Delete               | `Success`    | `Error`    | —                        |
| Move (env mode)      | `Success`    | `Error`    | —                        |
| Move (`--vars` mode) | `Success`    | `Error`    | `Warning` (skipped vars) |
| Copy                 | `Success`    | `Error`    | —                        |
| Unknown              | —            | `Error`    | —                        |

---

## Persistence / Side Effects

| Action               | Write to disk                                                       | Context mutation                                                       |
| -------------------- | ------------------------------------------------------------------- | ---------------------------------------------------------------------- |
| Show                 | None                                                                | None                                                                   |
| List                 | None                                                                | None                                                                   |
| Load                 | Saves current env variables before switching (`save_env_variables`) | `ctx.clear()` then re-populated from target env; `current_env` updated |
| Save                 | Writes variable snapshot to env store (`save_env_variables`)        | None                                                                   |
| New                  | Creates env entry + `config.save()`                                 | None                                                                   |
| Delete               | Removes env entry + `config.save()`                                 | None                                                                   |
| Move (env mode)      | Renames env (`config.rename_env`)                                   | None                                                                   |
| Move (`--vars` mode) | Writes subset to dest env + saves current env                       | Selected variables removed from `ctx` via `ctx.unset()`                |
| Copy                 | Duplicates env (`config.copy_env`)                                  | None                                                                   |

All data is stored in the JSON config file at `~/.config/cmath-solver/` on Linux or `%APPDATA%\cmath-solver\` on Windows. The REPL prompt reflects `current_env` in real time via `build_prompt(current_env)` in `src/ui/repl/runner.cpp`, so a successful `Load` immediately updates the visible prompt without requiring a restart.
