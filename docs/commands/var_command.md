# `:set` / `:unset` / `:rm` Commands — Detailed Walkthrough

## Table of Contents
1. [Overview](#1-overview)
2. [Step 1 — Input Parsing](#step-1--input-parsing)
3. [Step 2 — AST Construction](#step-2--ast-construction)
4. [Step 3 — Dispatch to Handler](#step-3--dispatch-to-handler)
5. [Step 4 — Action Execution](#step-4--action-execution)
6. [Reference Tables](#reference-tables)

---

## 1. Overview

The variable commands manage named bindings in the runtime context:

- **`:set <var> <expr>`** — parse `<expr>` and store it under `<var>`; evaluate eagerly if possible.
- **`:set <var> solve <equation>`** — solve the equation for `<var>` and store the numeric result.
- **`:set <var> expand <expr>`** — expand `<expr>` as a polynomial and store the canonical expanded form.
- **`:set <var> factor <expr>`** — factor `<expr>` as a polynomial and store the factored form.
- **`:unset <var>`** / **`:rm <var>`** — remove an existing variable binding from the context.

Variable names must be valid identifiers (start with a letter or `_`, contain only alphanumeric characters or `_`, and must not be a reserved keyword).

**Full pipeline for `:set x 2 + 3`:**

```
":set x 2 + 3"
  │
  ├─ Runner::run_line()                        ui/repl/runner.cpp
  │    └─ parse_command(line)
  │         └─ CommandParser::parse()          parser/command/command_parser.cpp
  │              ├─ CommandTokenStream("...")   lexer/command/
  │              └─ SubparserRegistry[":set"]  parser/command/command_parser_registry.cpp
  │                   └─ VarCommandParser::parse()
  │                        └─ VarCommand(Set, "x", "2 + 3")   ast/command/var_command.hpp
  │
  ├─ HandlerRegistry::dispatch(VarCommand)     commands/registry.cpp
  │    └─ visit(VarCommand, sink)
  │         └─ var_reg_.dispatch(Action::Set, …)
  │              └─ handlers::handle_var(…)    commands/handlers/var_handler.hpp
  │                   └─ handle_set(…)
  │                        └─ ctx.set("x", parse("2+3")); eval → "  x = 5"
  │
  └─ sink.flush(std::cout)                     diagnostics/sink.hpp
```

---

## Step 1 — Input Parsing

`Runner::run_line()` (`ui/repl/runner.cpp`) calls the free function `parse_command(line)`, which constructs a `CommandParser` and invokes `parse()`:

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

`CommandParser::parse()` (`parser/command/command_parser.cpp`) tokenizes the input with `CommandTokenStream`. When the first token has type `CommandTokenType::Command` (i.e. starts with `:`), it looks the token up in the static `SubparserRegistry`:

```cpp
// parser/command/command_parser.cpp
if (stream.peek_is(CommandTokenType::Command)) {
    std::string              cmd      = stream.peek().value;
    static SubparserRegistry registry = build_registry();
    auto                     it       = registry.find(cmd);
    if (it != registry.end()) {
        return it->second->parse(stream);
    }
    // Unknown colon command → SystemCommand::Unknown
    return Result<CommandPtr>::ok(std::make_unique<SystemCommand>(
        SystemCommand::Type::Unknown, raw_input_));
}
```

The `SubparserRegistry` (`parser/command/command_parser_registry.cpp`) maps all three command strings to the same `VarCommandParser` instance type:

```cpp
// parser/command/command_parser_registry.cpp
static const std::unordered_set<std::string> var_cmds = {
    ":set", ":unset", ":rm"};
for (const auto& cmd : var_cmds)
    reg[cmd] = std::make_unique<VarCommandParser>();
```

`:rm` is an alias for `:unset`; both are treated identically during parsing because the parser inspects the token value directly.

---

## Step 2 — AST Construction

`VarCommandParser::parse()` (`parser/command/subparsers/var_command_parser.cpp`) builds a `VarCommand` node:

```cpp
Result<CommandPtr> VarCommandParser::parse(ITokenStream& stream) {
    bool is_set = (stream.peek().value == ":set");
    stream.advance();                          // consume ":set" / ":unset" / ":rm"

    // Bare invocation (no variable name)
    if (stream.is_eof()) {
        return Result<CommandPtr>::ok(std::make_unique<VarCommand>(
            is_set ? VarCommand::Action::Set : VarCommand::Action::Unset,
            "", stream.raw_input()));
    }

    std::string var_name = stream.peek().value;
    stream.advance();                          // consume variable name

    auto var_cmd = std::make_unique<VarCommand>(
        is_set ? VarCommand::Action::Set : VarCommand::Action::Unset,
        var_name, stream.raw_input());

    if (is_set) {
        // Check for a recognized math-action keyword
        if (stream.peek_is(CommandTokenType::Word) &&
            (stream.peek().value == "solve" ||
             stream.peek().value == "expand" ||
             stream.peek().value == "factor")) {
            std::string math_action = stream.peek().value;
            stream.advance();
            var_cmd->set_payload(math_action, stream.consume_remaining());
        } else {
            // Default assignment — empty math_action, rest is the expression
            var_cmd->set_payload("", stream.consume_remaining());
        }
    }
    return Result<CommandPtr>::ok(std::move(var_cmd));
}
```

Key invariants:
- For `:unset` / `:rm`, `set_payload()` is **never called** — `has_payload()` and `has_math_action()` both return `false`.
- For `:set`, `set_payload()` is **always called** (unless the stream is at EOF after the action token, which means no variable name was given and we return early with an empty `var_name`).
- The parser performs **no validation** of the variable name or expression; all validation is deferred to the handler.

**Input-to-AST mapping:**

| Input                   | `action_` | `var_name_` | `math_action_` | `payload_`  |
| ----------------------- | --------- | ----------- | -------------- | ----------- |
| `:set`                  | `Set`     | `""`        | (not set)      | (not set)   |
| `:set x`                | `Set`     | `"x"`       | `""`           | `""`        |
| `:set x 2+3`            | `Set`     | `"x"`       | `""`           | `"2+3"`     |
| `:set x solve x = 5`    | `Set`     | `"x"`       | `"solve"`      | `"x = 5"`   |
| `:set x expand (x+1)^2` | `Set`     | `"x"`       | `"expand"`     | `"(x+1)^2"` |
| `:set x factor x^2-1`   | `Set`     | `"x"`       | `"factor"`     | `"x^2-1"`   |
| `:unset x`              | `Unset`   | `"x"`       | (not set)      | (not set)   |
| `:rm x`                 | `Unset`   | `"x"`       | (not set)      | (not set)   |
| `:unset`                | `Unset`   | `""`        | (not set)      | (not set)   |

---

## Step 3 — Dispatch to Handler

`HandlerRegistry::visit()` (`commands/registry.cpp`) forwards the node to `var_reg_`, a `CommandRegistry<VarCommand, VarCommand::Action>`:

```cpp
// commands/registry.cpp
void HandlerRegistry::visit(const VarCommand& cmd, DiagnosticSink& sink) {
    last_command_status_ = var_reg_.dispatch(cmd.action(), cmd, ctx_, cfg_,
                                             current_env_, sink);
}
```

All three action variants (`Set`, `Unset`, `Unknown`) are registered during `build_handler_registry()` with the same closure:

```cpp
// commands/registry.cpp
for (auto type : {VarCommand::Action::Set, VarCommand::Action::Unset,
                  VarCommand::Action::Unknown}) {
    reg.var().add(
        type,
        [](const VarCommand& cmd, Context& ctx, Config& cfg,
           std::string& env, DiagnosticSink& sink) -> HistoryStatus {
            return handlers::handle_var(cmd, ctx, cfg, env, sink);
        });
}
```

`handlers::handle_var()` (`commands/handlers/var_handler.hpp`) receives `cmd`, `ctx`, `cfg`, the raw env string `env`, and `sink`, then dispatches on `cmd.action()`.

---

## Step 4 — Action Execution

### `Action::Set` — `handle_set()`

Validation proceeds in order before any mutation occurs:

1. **Empty variable name** (`var_name_ == ""`): emits **E0401** (`missing_var_name`, usage `":set <var> <expr>"`), returns `Error`.
2. **Invalid identifier**: if `is_valid_identifier()` fails:
   - reserved keyword → **E0402** (`reserved_keyword`), returns `Error`.
   - invalid characters → **E0403** (`invalid_identifier`), returns `Error`.
3. **Empty payload** (`payload_` is empty string): emits **E0404** (`missing_expr`), returns `Error`.
4. **No math action set** (`!has_math_action()`): emits **E0404** (`missing_expr`), returns `Error`. (Safety guard; not reachable with a well-formed parse.)

After validation, the handler branches on `math_action_`:

#### `"solve"`

Parses the payload as an equation via `Parser::parse_equation()`. Creates a temporary `Context` that contains all variables **except** the target variable (to avoid self-reference). Calls `EquationSolver::solve()`, stores the result in `ctx`, and outputs:

```
  <var> = <solved_value>
```

Returns `Success`. Exceptions from `EquationSolver` are caught and return `Error` without diagnostic output (the solver's own error surfaces to the user).

#### `"expand"`

Parses the payload as an expression via `Parser::parse()`. Converts the AST to a `Polynomial` via `ASTToPolynomial::convert()`. Converts the expanded polynomial back to a string, re-parses it into an AST, and stores that AST in `ctx`. Outputs:

```
  <var> = <expanded_polynomial>
```

Returns `Success`. Parse or conversion failures return `Error` with the relevant diagnostic.

#### `"factor"`

Same flow as `"expand"` but calls `factor_polynomial()` after conversion. Stores the factored form in `ctx`. Outputs:

```
  <var> = <factored_form>
```

Returns `Success`. Failures return `Error` with the relevant diagnostic.

#### Default (empty `math_action_`)

Parses the payload as an expression via `Parser::parse()` and stores the raw AST in `ctx`. Then attempts **eager evaluation** via `Evaluator::evaluate()`. If evaluation succeeds, outputs the numeric result:

```
  <var> = <numeric_value>
```

Returns `Success`. If parsing or evaluation fails (e.g. the expression references an unbound variable), returns `Error`.

---

### `Action::Unset` — `handle_unset()`

Validation:

1. **Empty variable name** (`var_name_ == ""`): emits **E0401** (`missing_var_name`, usage `":unset <var>"`), returns `Error`.

If the variable exists in `ctx`, it is removed via `ctx.unset(var)` and the handler outputs:

```
  Removed: <var>
```

Returns `Success`.

If the variable does not exist, emits **E0425** (`var_not_found`). The diagnostic includes a fuzzy suggestion via `suggest()` (`ui/suggestions.hpp`) if a similarly-named variable exists in the context. Returns `Error`.

---

### `Action::Unknown` — `handle_var()` fallthrough

Reached when the command token was not `:set`, `:unset`, or `:rm` but was still routed to `VarCommandParser` (not reachable through normal dispatch since the registry maps only those three tokens). Extracts the bad command token from `cmd.raw_command()` and emits **E0002** (`unknown_command`) with help text listing available commands. Returns `Error`.

---

## Reference Tables

### Error Codes

| Code  | Function             | Trigger condition                               | Help text                                                                        |
| ----- | -------------------- | ----------------------------------------------- | -------------------------------------------------------------------------------- |
| E0401 | `missing_var_name`   | No variable name after `:set` or `:unset`       | Usage string for the command                                                     |
| E0402 | `reserved_keyword`   | Variable name is a reserved keyword             | "choose a different variable name"                                               |
| E0403 | `invalid_identifier` | Variable name fails identifier rules            | "names must start with a letter or `_` and contain only alphanumeric characters" |
| E0404 | `missing_expr`       | No expression after the variable name in `:set` | `Usage: ':set <var> <expr>'`                                                     |
| E0425 | `var_not_found`      | Variable given to `:unset` does not exist       | Fuzzy suggestion if a similarly-named variable exists                            |
| E0002 | `unknown_command`    | `Action::Unknown` reached                       | Lists available commands                                                         |

### Identifier Validation Rules (`is_valid_identifier`)

| Rule                 | Requirement                                       |
| -------------------- | ------------------------------------------------- |
| First character      | Must be `[A-Za-z_]`                               |
| Remaining characters | Must be `[A-Za-z0-9_]`                            |
| Reserved keywords    | Not allowed (checked via `is_reserved_keyword()`) |

### Recognized Math Actions in `:set`

| Keyword  | Operation                                     | Input type             | Output stored                              |
| -------- | --------------------------------------------- | ---------------------- | ------------------------------------------ |
| `solve`  | `EquationSolver::solve()`                     | Equation (`lhs = rhs`) | Numeric result                             |
| `expand` | `ASTToPolynomial` + `Polynomial::to_string()` | Expression             | Expanded polynomial AST                    |
| `factor` | `ASTToPolynomial` + `factor_polynomial()`     | Expression             | Factored polynomial AST                    |
| *(none)* | `Parser::parse()` + `Evaluator::evaluate()`   | Expression             | Raw expression AST; numeric result printed |
