# `:config` Command — Detailed Walkthrough

## Table of Contents
- [`:config` Command — Detailed Walkthrough](#config-command--detailed-walkthrough)
  - [Table of Contents](#table-of-contents)
  - [1. Overview](#1-overview)
  - [Step 1 — Input Parsing](#step-1--input-parsing)
    - [1.1 Routing](#11-routing)
    - [1.2 Subparser Lookup](#12-subparser-lookup)
  - [Step 2 — AST Construction](#step-2--ast-construction)
  - [Step 3 — Dispatch to Handler](#step-3--dispatch-to-handler)
  - [Step 4 — Action Execution](#step-4--action-execution)
    - [List](#list)
    - [Get](#get)
    - [Set](#set)
    - [Path](#path)
    - [Reset](#reset)
    - [Unknown](#unknown)
  - [Settings Reference](#settings-reference)
    - [Output](#output)
    - [Solver](#solver)
    - [History](#history)
    - [REPL](#repl)
    - [General](#general)
  - [Config File Structure](#config-file-structure)
    - [Loading \& Migration](#loading--migration)

---

## 1. Overview

`:config` manages runtime settings. It follows the same AST-based dispatch pipeline used by all other commands.

Both `:config` and `:conf` are valid (`:conf` is a legacy alias registered in `command_parser_registry.cpp`).

```
Input: ":config set output.decimals 3"
  │
  ├─ Runner::run_line()           (ui/repl/runner.cpp)
  │    └─ parse_command(line)
  │         └─ CommandParser::parse()
  │              └─ SubparserRegistry lookup → ConfigCommandParser::parse()
  │                   └─ Result<CommandPtr>  (ConfigCommand, Action::Set, key="output.decimals", value="3")
  │
  ├─ HandlerRegistry::dispatch(*cmd)
  │    └─ cmd.accept(*this, sink)           (double-dispatch via CommandVisitor)
  │         └─ HandlerRegistry::visit(const ConfigCommand&, DiagnosticSink&)
  │              └─ config_reg_.dispatch(Action::Set, cmd, ctx_, cfg_, current_env_, sink)
  │                   └─ handlers::handle_config(cmd, cfg, sink)
  │
  └─ Output: "  output.decimals = 3"
```

---

## Step 1 — Input Parsing

### 1.1 Routing

`Runner::run_line()` calls `parse_command(line)`, which constructs a `CommandParser` and calls `parse()`.

```cpp
// ui/repl/runner.cpp
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

### 1.2 Subparser Lookup

`CommandParser::parse()` tokenises the input into a `CommandTokenStream`. When the first token is a `Command`-type token (colon-prefixed), it looks up the keyword in a static `SubparserRegistry`:

```cpp
// parser/command/command_parser.cpp
if (stream.peek_is(CommandTokenType::Command)) {
    std::string              cmd      = stream.peek().value; // e.g. ":config"
    static SubparserRegistry registry = build_registry();
    auto                     it       = registry.find(cmd);
    if (it != registry.end())
        return it->second->parse(stream); // → ConfigCommandParser
    // Unknown colon-command falls back to SystemCommand::Unknown
}
```

Both `":config"` and `":conf"` are registered in `build_registry()` (in `command_parser_registry.cpp`) and both route to `ConfigCommandParser`.

---

## Step 2 — AST Construction

`ConfigCommandParser::parse()` advances past the command token, then reads the optional subcommand and arguments to build a `ConfigCommand` AST node.

```cpp
// parser/command/subparsers/config_command_parser.cpp
Result<CommandPtr> ConfigCommandParser::parse(ITokenStream& stream) {
    stream.advance(); // consume ":config" / ":conf"

    if (stream.is_eof())
        // Bare ":config" → defaults to List action
        return Result<CommandPtr>::ok(
            std::make_unique<ConfigCommand>(ConfigCommand::Action::List,
                                           stream.raw_input()));

    std::string sub = stream.peek().value; // "list", "get", "set", "path", "reset"
    stream.advance();

    ConfigCommand::Action action;
    if      (sub == "list")  action = ConfigCommand::Action::List;
    else if (sub == "get")   action = ConfigCommand::Action::Get;
    else if (sub == "set")   action = ConfigCommand::Action::Set;
    else if (sub == "path")  action = ConfigCommand::Action::Path;
    else if (sub == "reset") action = ConfigCommand::Action::Reset;
    else                     action = ConfigCommand::Action::Unknown;

    auto config_cmd = std::make_unique<ConfigCommand>(action, stream.raw_input());

    // For Unknown, stash the raw subcommand in key_ for error reporting
    if (action == ConfigCommand::Action::Unknown) {
        config_cmd->set_kv(sub, "");
        return Result<CommandPtr>::ok(std::move(config_cmd));
    }

    std::string key, value;
    if (!stream.is_eof()) { key = stream.peek().value; stream.advance(); }
    // Only Set reads a value; other actions ignore trailing tokens
    if (action == ConfigCommand::Action::Set && !stream.is_eof())
        value = stream.consume_remaining();

    config_cmd->set_kv(key, value);
    return Result<CommandPtr>::ok(std::move(config_cmd));
}
```

**Resulting AST node (`ConfigCommand`):**

| Input                           | `action_` | `key_`              | `value_` |
| ------------------------------- | --------- | ------------------- | -------- |
| `:config`                       | `List`    | `""`                | `""`     |
| `:config list`                  | `List`    | `""`                | `""`     |
| `:config get output.decimals`   | `Get`     | `"output.decimals"` | `""`     |
| `:config set output.decimals 3` | `Set`     | `"output.decimals"` | `"3"`    |
| `:config path`                  | `Path`    | `""`                | `""`     |
| `:config reset`                 | `Reset`   | `""`                | `""`     |
| `:config foo`                   | `Unknown` | `"foo"`             | `""`     |

---

## Step 3 — Dispatch to Handler

`HandlerRegistry::dispatch()` calls `cmd.accept(*this, sink)`, which double-dispatches to the correct `visit()` overload:

```cpp
// commands/registry.cpp
void HandlerRegistry::visit(const ConfigCommand& cmd, DiagnosticSink& sink) {
    last_command_status_ = config_reg_.dispatch(
        cmd.action(), cmd, ctx_, cfg_, current_env_, sink);
}
```

`config_reg_` is a `CommandRegistry<ConfigCommand, ConfigCommand::Action>`. The handler closure registered for every config action calls:

```cpp
handlers::handle_config(cmd, cfg, sink)
```

---

## Step 4 — Action Execution

`handle_config()` is a single switch over `cmd.action()` in `commands/handlers/config_handler.hpp`.

### List

Iterates `Settings::all_keys()` and calls `config.settings().get(key)` for each, then formats the result into a two-column aligned table printed via `sink.push_output()`.

```
  Settings
  output.mode                 auto
  output.decimals             6
  ...
```

### Get

```cpp
case ConfigCommand::Action::Get: {
    const std::string& key = cmd.key();
    if (key.empty()) {
        sink.push(errors::missing_setting_key(raw, "get", "`config get <key>`", file, line));
        return HistoryStatus::Error;
    }
    if (!Settings::is_valid_key(key)) {
        sink.push(errors::unknown_setting(raw, key, file, line));
        return HistoryStatus::Error;
    }
    oss << "  " << key << " = " << config.settings().get(key) << "\n";
    sink.push_output(oss.str());
    return HistoryStatus::Success;
}
```

On an unknown key, `errors::unknown_setting` runs `suggest(key, Settings::all_keys())` (Levenshtein-based fuzzy match in `ui/suggestions.hpp`) and emits either a "did you mean" help or the full list of valid categories.

### Set

```cpp
case ConfigCommand::Action::Set: {
    if (key.empty() || value.empty()) { /* E0502 */ return HistoryStatus::Error; }
    if (!Settings::is_valid_key(key)) { /* E0503 */ return HistoryStatus::Error; }

    // auto_load_env: the named environment must already exist — hard error
    if (key == "auto_load_env" && !value.empty() && !config.env_exists(value)) {
        sink.push(errors::env_ref_error(raw, value, config, file, line));
        return HistoryStatus::Error;
    }

    std::string err_msg = config.settings().set(key, value);
    if (!err_msg.empty()) {
        sink.push(errors::invalid_setting_value(raw, key, value, err_msg, file, line));
        return HistoryStatus::Error;
    }
    config.save();           // write to disk immediately
    sink.push_output("  " + key + " = " + config.settings().get(key) + "\n");
    return HistoryStatus::Success;
}
```

`Settings::set()` validates and mutates the struct field. The helper closures `parse_bool` and `parse_int` (with explicit range bounds) are used internally:

| Key                     | Validation                                               |
| ----------------------- | -------------------------------------------------------- |
| `output.mode`           | must be `"auto"`, `"decimal"`, or `"exact"`              |
| `output.decimals`       | integer, 0–15                                            |
| `output.fraction`       | bool: `true`/`false`/`1`/`0`/`on`/`off`                  |
| `output.trailing_zeros` | bool                                                     |
| `output.thousands_sep`  | bool                                                     |
| `solver.tolerance`      | `stod`, must be `> 0`                                    |
| `solver.max_iter`       | integer, 1–1,000,000                                     |
| `history.size`          | integer, 1–100,000                                       |
| `history.dedup`         | bool                                                     |
| `history.ignore`        | any string (no validation)                               |
| `repl.prompt`           | any string (no validation)                               |
| `repl.show_timing`      | bool                                                     |
| `repl.auto_save_env`    | bool                                                     |
| `repl.confirm_delete`   | bool                                                     |
| `auto_load_env`         | validated in handler (env must exist); then stored as-is |

On success, `config.save()` serialises the current settings and all environments to disk at `config.file_path()`.

### Path

```cpp
case ConfigCommand::Action::Path:
    oss << "  " << config.file_path() << "\n";
    sink.push_output(oss.str());
    return HistoryStatus::Info;
```

Path resolution order (in `Config::resolve_config_path()`):
1. `./math_solver.json` in current working directory (if the file already exists)
2. Platform config directory:
   - **Windows**: `%APPDATA%\math-solver\math_solver.json`
   - **Linux / other**: `$HOME/.config/math-solver/math_solver.json`
3. Fallback to `./math_solver.json` if no platform variable is set

### Reset

```cpp
case ConfigCommand::Action::Reset:
    config.reset_settings(); // replaces settings_ with Settings{}
    config.save();
    sink.push_output("  Settings reset to defaults\n");
    return HistoryStatus::Success;
```

Only resets settings — environments are not affected.

### Unknown

```cpp
case ConfigCommand::Action::Unknown:
    sink.push(errors::unknown_config_subcommand(raw, cmd.key(), file, line));
    return HistoryStatus::Error;
```

`errors::unknown_config_subcommand` runs fuzzy matching against `{"list", "get", "set", "path", "reset"}` and emits a "did you mean" suggestion when possible.

---

## Settings Reference

### Output

| Key                     | Type   | Default  | Valid values                      |
| ----------------------- | ------ | -------- | --------------------------------- |
| `output.mode`           | string | `"auto"` | `auto`, `decimal`, `exact`        |
| `output.decimals`       | int    | `6`      | 0–15                              |
| `output.fraction`       | bool   | `false`  | `true`/`false`/`on`/`off`/`1`/`0` |
| `output.trailing_zeros` | bool   | `false`  | same as above                     |
| `output.thousands_sep`  | bool   | `false`  | same as above                     |

### Solver

| Key                | Type   | Default | Valid values        |
| ------------------ | ------ | ------- | ------------------- |
| `solver.tolerance` | double | `1e-12` | any positive number |
| `solver.max_iter`  | int    | `1000`  | 1–1,000,000         |

`solver.tolerance` is displayed in the `get`/`list` output as `"1e-N"` (rounded base-10 exponent), not as the raw float.

### History

| Key              | Type   | Default | Valid values |
| ---------------- | ------ | ------- | ------------ |
| `history.size`   | int    | `1000`  | 1–100,000    |
| `history.dedup`  | bool   | `true`  | bool         |
| `history.ignore` | string | `""`    | any string   |

### REPL

| Key                   | Type   | Default | Valid values |
| --------------------- | ------ | ------- | ------------ |
| `repl.prompt`         | string | `"> "`  | any string   |
| `repl.show_timing`    | bool   | `false` | bool         |
| `repl.auto_save_env`  | bool   | `true`  | bool         |
| `repl.confirm_delete` | bool   | `true`  | bool         |

### General

| Key             | Type   | Default | Notes                                                                                                 |
| --------------- | ------ | ------- | ----------------------------------------------------------------------------------------------------- |
| `auto_load_env` | string | `""`    | The named environment must already exist; attempting to set it to a non-existent name is a hard error |

---

## Config File Structure

Config is stored as a single JSON file. `Config::save()` serialises settings and all environments together (pretty-printed, indent = 2):

```json
{
    "settings": {
        "output": {
        "mode": "auto",
        "decimals": 6,
        "fraction": false,
        "trailing_zeros": false,
        "thousands_sep": false
    },
    "solver": {
        "tolerance": 1e-12,
        "max_iter": 1000
    },
    "history": {
        "size": 1000,
        "dedup": true,
        "ignore": ""
    },
    "repl": {
        "prompt": "> ",
        "show_timing": false,
        "auto_save_env": true,
        "confirm_delete": true
        },
    "auto_load_env": ""
    },
    "environments": {
        "default": {
            "variables": {
                "pi": "3.14159265358979",
                "e": "2.71828182845905",
                "tau": "6.28318530717959"
            }
        }
    }
}
```

Environment variables are stored as **strings** (not numbers). `Environment::from_json()` handles legacy numeric values by converting them to string and stripping trailing zeros.

### Loading & Migration

`Config::load()` detects legacy flat-key format (`precision`, `fraction_mode`, `history_size`) by checking for their presence at the top-level settings object, and routes to `Settings::migrate_legacy()` instead of `Settings::from_json()`. Missing keys always fall back to the struct's default values.

On first run (config file absent), `Config::create_defaults()` is called, which creates the default settings and a `"default"` environment pre-populated with `pi`, `e`, and `tau`.
