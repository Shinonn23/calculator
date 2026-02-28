# Math Commands — Detailed Walkthrough

## Table of Contents

1. [Overview](#1-overview)
2. [Step 1 — Input Routing](#step-1--input-routing)
3. [Step 2 — AST Construction](#step-2--ast-construction)
4. [Step 3 — The Math Lexer](#step-3--the-math-lexer)
5. [Step 4 — The Expression Parser](#step-4--the-expression-parser)
6. [Step 5 — Dispatch to Handler](#step-5--dispatch-to-handler)
7. [Step 6 — Action Execution](#step-6--action-execution)
8. [Reference Tables](#reference-tables)
9. [Persistence / Side Effects](#persistence--side-effects)

---

## 1. Overview

Math commands are the core feature of Math Solver. They cover expression
evaluation, equation solving, linear simplification, polynomial expansion, and
polynomial factoring. All five operations share a single AST node
(`MathCommand`) and a single handler entry point (`handle_math`). They differ
only in the `MathCommand::Type` enum value produced during parsing and the
algebra engine called during execution.

Bare input with no colon prefix is automatically treated as an evaluate
request; no command keyword is required.

```
Input: ":solve 2*x + 4 = 10"
  │
  ├─ parse_command()                          parser/command/command_parser.cpp
  │    └─ CommandParser::parse()
  │         ├─ CommandTokenStream             lexer/command/command_token_stream.hpp
  │         └─ SubparserRegistry[":solve"]    parser/command/command_parser_registry.cpp
  │              └─ MathCommandParser::parse()
  │                   parser/command/subparsers/math_command_parser.cpp
  │                   └─ MathCommand(Type::Solve, "2*x + 4 = 10", raw)
  │
  ├─ HandlerRegistry::visit(MathCommand, sink)   commands/registry.cpp
  │    └─ math_reg_.dispatch(Type::Solve, ...)
  │         └─ handlers::handle_math(cmd, ctx, cfg, sink)
  │              commands/handlers/math_handler.hpp
  │              └─ do_solve("2*x + 4 = 10", ...)
  │                   ├─ Parser::parse_equation()   parser/math/math_parser.cpp
  │                   ├─ LinearCollector            algebra/linear/linear_collector.hpp
  │                   ├─ EquationSolver::solve()    algebra/solver/solver.hpp
  │                   └─ ctx.set("x", 3.0)          runtime/context/context.hpp
  │
  └─ sink output: "  x = 3 (saved)"
```

---

## Step 1 — Input Routing

`Runner::run_line()` calls `parse_command(line)`, which constructs a
`CommandParser` and calls `CommandParser::parse()`.

```cpp
// parser/command/command_parser.cpp

CommandParser::CommandParser(const std::string& input)
    : raw_input_(strip_comment(input)) {}

Result<CommandPtr> CommandParser::parse() {
    CommandTokenStream stream(raw_input_);

    // Empty input (after comment stripping) → synthesized Evaluate command.
    if (stream.is_eof()) {
        return Result<CommandPtr>::ok(std::make_unique<MathCommand>(
            MathCommand::Type::Evaluate, "", raw_input_));
    }

    // Colon-prefixed command → look up in static SubparserRegistry.
    if (stream.peek_is(CommandTokenType::Command)) {
        std::string              cmd      = stream.peek().value;
        static SubparserRegistry registry = build_registry();
        auto                     it       = registry.find(cmd);
        if (it != registry.end()) {
            return it->second->parse(stream);   // MathCommandParser for math commands
        }
        // Unknown colon-command → fall through to SystemCommand::Unknown.
        return Result<CommandPtr>::ok(std::make_unique<SystemCommand>(
            SystemCommand::Type::Unknown, raw_input_));
    }

    // Legacy bare-word system commands (exit, quit, help, …).
    { ... }

    // Final fallback: bare expression or anything else → Evaluate.
    return Result<CommandPtr>::ok(std::make_unique<MathCommand>(
        MathCommand::Type::Evaluate, trim(raw_input_), raw_input_));
}
```

The `SubparserRegistry` (`parser/command/command_parser_registry.cpp`) maps the
four math command keywords to `MathCommandParser`:

```cpp
static const std::unordered_set<std::string> math_cmds = {
    ":solve", ":simplify", ":expand", ":factor"};
for (const auto& cmd : math_cmds)
    reg[cmd] = std::make_unique<MathCommandParser>();
```

`Type::Evaluate` is **never** produced by `MathCommandParser` — it is injected
directly by `CommandParser::parse()` for the empty-input path and the bare-
expression fallback.

---

## Step 2 — AST Construction

### MathCommand node

`MathCommand` (`ast/command/math_command.hpp`) is the sole command AST node for
all math operations:

```cpp
class MathCommand : public Command {
public:
    enum class Type { Evaluate, Solve, Simplify, Expand, Factor, Unknown };

private:
    Type                     type_;
    std::string              payload_;       // expression or equation string
    std::vector<std::string> specific_vars_; // populated by --vars
    bool                     isolated_    = false;
    bool                     as_fraction_ = false;
    ...
};
```

### MathCommandParser::parse()

For the four colon-prefixed keywords, `MathCommandParser::parse()` runs:

```cpp
// parser/command/subparsers/math_command_parser.cpp

static MathCommand::Type resolve_type(const std::string& cmd) {
    if (cmd == ":solve")    return MathCommand::Type::Solve;
    if (cmd == ":simplify") return MathCommand::Type::Simplify;
    if (cmd == ":expand")   return MathCommand::Type::Expand;
    return MathCommand::Type::Factor;  // default (":factor")
}

Result<CommandPtr> MathCommandParser::parse(ITokenStream& stream) {
    MathCommand::Type type = resolve_type(stream.peek().value);
    stream.advance();   // consume the command token

    bool                     isolated = false, fraction = false;
    std::vector<std::string> vars;

    // Single-pass flag parsing; order is not significant.
    while (stream.peek_is(CommandTokenType::Flag)) {
        std::string flag = stream.advance().value;
        if (flag == "-isolated" || flag == "--isolated")
            isolated = true;
        else if (flag == "-fraction" || flag == "--fraction")
            fraction = true;
        else if (flag == "-vars" || flag == "--vars") {
            // Greedy: consume all subsequent Word/QuotedString tokens.
            while (stream.peek_is(CommandTokenType::Word) ||
                   stream.peek_is(CommandTokenType::QuotedString)) {
                vars.push_back(stream.advance().value);
            }
        }
    }

    auto cmd = std::make_unique<MathCommand>(
        type, stream.consume_remaining(), stream.raw_input());
    cmd->set_flags(isolated, fraction, vars);
    return Result<CommandPtr>::ok(std::move(cmd));
}
```

`stream.consume_remaining()` collects everything after the flags as the raw
expression/equation string stored in `payload_`.

### Input → AST field mapping

| Input                                     | `type_`    | `payload_`         | `isolated_` | `as_fraction_` | `specific_vars_` |
| ----------------------------------------- | ---------- | ------------------ | ----------- | -------------- | ---------------- |
| `2 + 3 * x`                               | `Evaluate` | `"2 + 3 * x"`      | false       | false          | `[]`             |
| `:solve 2*x + 4 = 10`                     | `Solve`    | `"2*x + 4 = 10"`   | false       | false          | `[]`             |
| `:simplify 3*x + 2*x = 10 --vars x`       | `Simplify` | `"3*x + 2*x = 10"` | false       | false          | `["x"]`          |
| `:simplify a*x = b --isolated --fraction` | `Simplify` | `"a*x = b"`        | true        | true           | `[]`             |
| `:expand (x + 1)^2`                       | `Expand`   | `"(x + 1)^2"`      | false       | false          | `[]`             |
| `:factor x^2 - 5*x + 6`                   | `Factor`   | `"x^2 - 5*x + 6"`  | false       | false          | `[]`             |
| `""` (empty)                              | `Evaluate` | `""`               | false       | false          | `[]`             |

---

## Step 3 — The Math Lexer

`Parser` (`parser/math/math_parser.hpp`) owns a `Lexer`
(`lexer/math/math_lexer.hpp`) that tokenizes the `payload_` string.

The lexer is single-pass with no backtracking. It produces tokens of these
types (`lexer/math/math_token.hpp`):

| `TokenType`  | Description                                                                 |
| ------------ | --------------------------------------------------------------------------- |
| `Number`     | Numeric literal (integer or float); `value` field holds the parsed `double` |
| `Identifier` | ASCII identifier; `name` field holds the string                             |
| `Plus`       | `+`                                                                         |
| `Minus`      | `-`                                                                         |
| `Mul`        | `*`                                                                         |
| `Div`        | `/`                                                                         |
| `Pow`        | `^`                                                                         |
| `LParen`     | `(`                                                                         |
| `RParen`     | `)`                                                                         |
| `Equals`     | `=`                                                                         |
| `Bang`       | `!`                                                                         |
| `End`        | End of input                                                                |

**Reserved keywords** are rejected as identifiers with error code `E0001`
(`errors::parse`): `simplify`, `solve`, `set`, `unset`, `clear`, `help`,
`exit`, `quit`, `config`, `env`, `expand`, `factor`.

**Leading-dot floats** are accepted (`.5` is valid, but bare `.` is rejected
with `E0001`).

**Unrecognized characters** produce `E0001` immediately; no recovery is
attempted.

---

## Step 4 — The Expression Parser

`Parser` (`parser/math/math_parser.cpp`) is a recursive-descent parser using
precedence climbing. Precedence levels from lowest to highest:

| Level       | Method                   | Operators / rule                                       |
| ----------- | ------------------------ | ------------------------------------------------------ |
| 1 (lowest)  | `parse_additive()`       | `+`, `-` — left-associative                            |
| 2           | `parse_multiplicative()` | `*`, `/`, implicit juxtaposition — left                |
| 3           | `parse_power()`          | `^` — **right**-associative                            |
| 4           | `parse_unary()`          | unary `-` (emits `UnaryOp::Neg`), unary `+` (identity) |
| 5 (highest) | `parse_primary()`        | number literal, identifier, `(expr)`                   |

**Implicit multiplication** is recognized in `parse_multiplicative()` whenever
the next token is `Number`, `Identifier`, or `LParen` after a complete right
operand — e.g. `2x`, `3(x+1)`, `xy`. No token is consumed for the implicit
operator; the next call to `parse_power()` consumes the start of the right
operand.

**Public entry points:**

| Method                                   | Accepts                | Returns                                                        |
| ---------------------------------------- | ---------------------- | -------------------------------------------------------------- |
| `Parser::parse()`                        | Single expression      | `Result<ExprPtr>`                                              |
| `Parser::parse_expression_or_equation()` | Expression OR equation | `Result<pair<ExprPtr, EquationPtr>>` — exactly one is non-null |
| `Parser::parse_equation()`               | Equation (`lhs = rhs`) | `Result<EquationPtr>`                                          |

All three reject trailing tokens (returns `E0001` "unexpected input after
…").

### AST node types

| Class      | Inherits | Key fields                                                                      |
| ---------- | -------- | ------------------------------------------------------------------------------- |
| `Number`   | `Expr`   | `value_: double`                                                                |
| `Variable` | `Expr`   | `name_: string`                                                                 |
| `UnaryOp`  | `Expr`   | `operand_: ExprPtr`, `op_: UnaryOpType` (`Neg`)                                 |
| `BinaryOp` | `Expr`   | `left_`, `right_: ExprPtr`, `op_: BinaryOpType` (`Add`/`Sub`/`Mul`/`Div`/`Pow`) |
| `Equation` | —        | `lhs_`, `rhs_: ExprPtr` — **not** an `Expr` subclass; prevents nesting          |

All `Expr` subclasses implement `accept(ExprVisitor&)` for the visitor pattern,
`to_string()` for diagnostics, and `clone()` for deep copy.

---

## Step 5 — Dispatch to Handler

`HandlerRegistry::visit()` receives the `MathCommand` node and calls
`math_reg_.dispatch()`:

```cpp
// commands/registry.cpp

void HandlerRegistry::visit(const MathCommand& cmd, DiagnosticSink& sink) {
    // math_reg_ must be fully populated for all MathCommand::Type variants.
    last_command_status_ =
        math_reg_.dispatch(cmd.type(), cmd, ctx_, cfg_, current_env_, sink);
}
```

In `build_handler_registry()`, all six `MathCommand::Type` values are
registered to the same closure, which calls `handlers::handle_math()`:

```cpp
for (auto type :
     {MathCommand::Type::Solve, MathCommand::Type::Simplify,
      MathCommand::Type::Expand, MathCommand::Type::Factor,
      MathCommand::Type::Evaluate, MathCommand::Type::Unknown}) {
    reg.math().add(type,
        [](const MathCommand& cmd, Context& ctx, Config& cfg,
           std::string& /*env*/, DiagnosticSink& sink) -> HistoryStatus {
            return handlers::handle_math(cmd, ctx, cfg, sink);
        });
}
```

`handle_math` receives `cmd`, `ctx`, `cfg`, and `sink`. It does **not** receive
`current_env_` (the lambda drops it).

---

## Step 6 — Action Execution

`handle_math` (`commands/handlers/math_handler.hpp`) switches on `cmd.type()`
and delegates to a dedicated inline function:

```cpp
inline HistoryStatus handle_math(const MathCommand& cmd, Context& ctx,
                                 Config& config, DiagnosticSink& sink) {
    const std::string& payload = cmd.payload();
    switch (cmd.type()) {
    case MathCommand::Type::Solve:    return do_solve   (payload, cmd, ctx, config, sink);
    case MathCommand::Type::Simplify: return do_simplify(payload, cmd, ctx, config, sink);
    case MathCommand::Type::Expand:   return do_expand  (payload, cmd, ctx, sink);
    case MathCommand::Type::Factor:   return do_factor  (payload, cmd, ctx, sink);
    case MathCommand::Type::Evaluate: return do_evaluate(payload, cmd, ctx, config, sink);
    case MathCommand::Type::Unknown:  { /* error */ } return HistoryStatus::Error;
    }
    return HistoryStatus::Unknown;
}
```

---

### `Type::Evaluate` — `do_evaluate`

Evaluates a bare expression or checks whether an equation holds numerically.

**Validation and execution sequence:**

1. Empty payload → returns `HistoryStatus::Error` immediately (no output).
2. Parses payload with `Parser::parse_expression_or_equation()`.
   - Failure → pushes `E0001` diagnostic, returns `Error`.
3. If the result is an **equation** (`lhs = rhs`):
   - Creates `Evaluator(&ctx, payload, &sink)`.
   - Evaluates `lhs` and `rhs` numerically. Any evaluation error is pushed to
     `sink`; if `sink.error_count()` increased, returns `Error`.
   - Checks `|lhs - rhs| < 1e-12`.
   - Prints `"  <lhs_val> = <rhs_val>  (true)"` in green or `"(false)"` in
     red.
   - Returns `Success`.
4. If the result is an **expression**:
   - Creates `Evaluator(&ctx, payload, &sink)`.
   - Evaluates the expression. Errors → `Error`.
   - Prints `"  = <val>"`.
   - Returns `Success`.

**No context mutation.** `Evaluator` performs lazy variable resolution from
`ctx` at evaluation time using the visitor pattern
(`src/eval/evaluator.hpp`).

---

### `Type::Solve` — `do_solve`

Solves a linear equation for a single unknown and stores the result.

**Validation and execution sequence:**

1. Empty payload → prints usage `"  Usage: :solve <lhs> = <rhs>\n"`, returns
   `Error`.
2. Parses payload with `Parser::parse_equation()`.
   - Failure → pushes `E0001`, returns `Error`.
3. Infers the set of unknowns using `LinearCollector` with the current context:
   - Collects `lhs` and `rhs` linear forms. On success, unknowns = variables
     in `(lhs_form - rhs_form)`.
   - On exception, pushes `E0200` diagnostic (non-fatal; unknown set remains
     empty and the fallback runs).
4. If unknowns is empty (non-linear or malformed), retries `LinearCollector`
   with `context = nullptr` to determine unknowns from the expression alone.
5. If exactly one unknown is found and that variable already exists in `ctx`,
   builds a temporary context (`temp_ctx`) containing all context bindings
   **except** the target variable, to avoid accidental shadowing.
6. Calls `EquationSolver(&solve_ctx, payload).solve(*eq)`.
   - Failure → pushes the solver's diagnostic with source location, returns
     `Error`. Relevant solver error codes: `E0301` (no solution), `E0302`
     (infinite solutions), `E0303` (invalid equation), `E0304` (multiple
     unknowns), `E0308` (non-linear).
7. On success:
   - Calls `ctx.set(result.variable, result.value)` — **mutates context**.
   - Prints `"  <variable> = <value> (saved)"` with the `(saved)` part dimmed.
   - Returns `Success`.

---

### `Type::Simplify` — `do_simplify`

Canonicalizes a linear equation to `Ax + By + ... = C` form.

**Validation and execution sequence:**

1. Empty payload → prints usage `"  Usage: :simplify <lhs> = <rhs> [--vars x y] [--isolated] [--fraction]\n"`, returns `Error`.
2. Parses payload with `Parser::parse_equation()`.
   - Failure → pushes `E0001`, returns `Error`.
3. Builds `SimplifyOptions`:
   - `opts.var_order = cmd.specific_vars()` (from `--vars`)
   - `opts.isolated = cmd.isolated()` (from `--isolated`)
   - `opts.as_fraction = cmd.as_fraction() || config.settings().output_fraction` (from `--fraction` or global config)
4. Calls `Simplifier(&ctx, payload).simplify(*eq, opts)`.
   - **Shadowing check** (when context is non-null and not isolated):
     `LinearCollector` with `context = nullptr` identifies all variable names.
     Any name present in `ctx` → warning pushed: `"'<var>' in expression
     shadows context variable (use --isolated to keep as variable)"`.
   - **Main collection**: `LinearCollector(isolated ? nullptr : &ctx, ...)`.
   - If collection fails (non-linear expression) → warning `"simplify failed:
     expression is not linear"` inserted into `result.warnings`, empty
     `canonical` returned.
   - Variable ordering: user-specified via `--vars`, or lexicographically
     sorted for determinism.
   - Normalizes to `(lhs - rhs) = 0`, then formats as `Ax + By + ... = C`.
5. For each warning in `result.warnings`, pushes a `Diagnostic::warning`
   to `sink` (no error code — warnings use a different factory).
6. Appends canonical form to `sink`. Then:
   - If `result.is_no_solution()` (form is constant, |constant| > 1e-12) →
     appends `"  => no solution"` in red.
   - If `result.is_infinite_solutions()` (form is constant, |constant| < 1e-12) →
     appends `"  => infinite solutions"` in green.
7. Returns `Warning` if any warnings were emitted, `Success` otherwise.

**No context mutation.**

---

### `Type::Expand` — `do_expand`

Expands a polynomial expression into its canonical distributed form.

**Validation and execution sequence:**

1. Empty payload → prints `"  Usage: :expand <expr>\n"`, returns `Error`.
2. Parses payload with `Parser::parse()` (expression only, no `=`).
   - Failure → pushes `E0001`, returns `Error`.
3. Calls `ASTToPolynomial(payload).convert(*expr)`.
   - Failure → pushes `E0310` (polynomial error) with source location, returns
     `Error`.
4. Prints `"  " + poly.to_string() + "\n"`.
5. Returns `Success`.

**No context mutation.**

---

### `Type::Factor` — `do_factor`

Factors a polynomial expression.

**Validation and execution sequence:**

1. Empty payload → prints `"  Usage: :factor <expr>\n"`, returns `Error`.
2. Parses payload with `Parser::parse()`.
   - Failure → pushes `E0001`, returns `Error`.
3. Calls `ASTToPolynomial(payload).convert(*expr)`.
   - Failure → pushes `E0310`, returns `Error`.
4. Calls `factor_polynomial(poly)`.
5. Prints `"  " + factored.to_string() + "\n"`.
6. Returns `Success`.

**No context mutation.** Factorization may be expensive for high-degree
polynomials.

---

### `Type::Unknown` — error path

Reached when `MathCommandParser` sets `Type::Unknown` (currently unreachable
from normal dispatch since `resolve_type` defaults to `Factor`, but kept for
forward compatibility and defensive correctness).

```cpp
case MathCommand::Type::Unknown: {
    std::string raw = cmd.raw_command();
    std::string bad = raw.substr(0, raw.find(' '));
    Diagnostic  d   = errors::unknown_command(
        bad, find_token_span(raw, bad), raw);  // E0002
    d = d.with_location(cmd.source_file(), cmd.source_line());
    sink.push(d);
}
return HistoryStatus::Error;
```

Error code: `E0002`. Help text: `"available commands: :help, :env, :config, :history, etc."`. No fuzzy suggestion is performed by `do_evaluate` itself (contrast with `:config`, `:env`, which call `suggest()`).

---

## Reference Tables

### MathCommand::Type — command routing

| `Type`     | Trigger                                           | Parser entry point                       |
| ---------- | ------------------------------------------------- | ---------------------------------------- |
| `Evaluate` | Bare input or empty input                         | `parse_expression_or_equation()` / empty |
| `Solve`    | `:solve <equation>`                               | `parse_equation()`                       |
| `Simplify` | `:simplify <equation>`                            | `parse_equation()`                       |
| `Expand`   | `:expand <expr>`                                  | `parse()`                                |
| `Factor`   | `:factor <expr>` (also default in `resolve_type`) | `parse()`                                |
| `Unknown`  | Defensive fallback; not reachable normally        | — (uses `cmd.raw_command()`)             |

### Flags (`:solve`, `:simplify` only)

| Flag (short / long)        | Field set        | Applicable to | Effect                                                        |
| -------------------------- | ---------------- | ------------- | ------------------------------------------------------------- |
| `-isolated` / `--isolated` | `isolated_`      | `:simplify`   | Variables not resolved from context; shadowing  check skipped |
| `-fraction` / `--fraction` | `as_fraction_`   | `:simplify`   | Coefficients and constants printed as fractions               |
| `-vars` / `--vars` <names> | `specific_vars_` | `:simplify`   | User-specified variable ordering in output                    |

`--isolated` and `--fraction` are parsed by `MathCommandParser` for all
subcommand types but are only meaningful for `Simplify`. The `do_solve`,
`do_expand`, `do_factor`, and `do_evaluate` functions ignore them.

### Error codes

| Code    | Constructor                      | When emitted                                                                                                              |
| ------- | -------------------------------- | ------------------------------------------------------------------------------------------------------------------------- |
| `E0001` | `errors::parse()`                | Lexer or parser failure (unexpected token, syntax error)                                                                  |
| `E0002` | `errors::unknown_command()`      | `Type::Unknown` action in `handle_math`                                                                                   |
| `E0200` | `Diagnostic::make(..., "E0200")` | Runtime exception catch-all in all `do_*` functions                                                                       |
| `E0301` | `errors::no_solution()`          | Solver: equation has no solution                                                                                          |
| `E0302` | `errors::infinite_solutions()`   | Solver: equation is a tautology                                                                                           |
| `E0303` | `errors::invalid_equation()`     | Solver: malformed equation                                                                                                |
| `E0304` | `errors::multiple_unknowns()`    | Solver: more than one unknown remains after context substitution; help: `"provide values for other variables using :set"` |
| `E0308` | `errors::non_linear()`           | Solver or collector: non-linear term detected                                                                             |
| `E0310` | `errors::polynomial()`           | `ASTToPolynomial::convert()` failure in `:expand` / `:factor`                                                             |

### HistoryStatus values returned

| Status    | Returned by                                                   |
| --------- | ------------------------------------------------------------- |
| `Success` | All `do_*` on success                                         |
| `Error`   | All `do_*` on empty payload, parse failure, or solver failure |
| `Warning` | `do_simplify` when `result.warnings` is non-empty             |
| `Unknown` | `handle_math` fall-through guard (should never be reached)    |

---

## Persistence / Side Effects

**Context mutation (`:solve` only):** `do_solve` calls `ctx.set(variable,
value)` on a successful solve. This writes a `Number` `ExprPtr` into the
persistent `Context` map, making the solved value available to subsequent
commands in the same REPL session.

**History:** Every math command dispatched through `HandlerRegistry` is
appended to `session_history_` via `push_history()` and written to the on-disk
history file by `handlers::append_history_file()`. The `HistoryStatus` returned
by `handle_math` is stored alongside the command text.

**No config writes.** Math commands read `config.settings().output_fraction`
(in `do_simplify`) but never call `config.save()`.

**No environment writes.** Environment state is unaffected by math commands.
