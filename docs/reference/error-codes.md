# Error Codes Reference

> **Complete reference of all diagnostic error codes emitted by cmath-solver.**
>
> All error codes follow the pattern `Exxxx`. The first digit(s) indicate the subsystem.

## Error Code Ranges

| Range         | Subsystem        | Source Header                            |
| ------------- | ---------------- | ---------------------------------------- |
| `E0000–E0099` | Math expressions | `src/diagnostics/kinds/math_kind.hpp`    |
| `E0100–E0199` | Solver           | `src/diagnostics/kinds/solver_kind.hpp`  |
| `E0200–E0299` | Command parsing  | `src/diagnostics/kinds/command_kind.hpp` |
| `E0300–E0399` | Equation solving | `src/diagnostics/kinds/solver_kind.hpp`  |
| `E0400–E0499` | Variables        | `src/diagnostics/kinds/var_kind.hpp`     |
| `E0500–E0599` | Config/Settings  | `src/diagnostics/kinds/config_kind.hpp`  |
| `E0600–E0699` | Environments     | `src/diagnostics/kinds/env_kind.hpp`     |
| `E0700–E0799` | History          | `src/diagnostics/kinds/history_kind.hpp` |
| `E0800–E0899` | Runtime          | `src/diagnostics/kinds/runtime_kind.hpp` |

---

## Math Expression Errors (`E0000–E0099`)

### `E0000` — Math Error

General math errors such as division by zero.

```
error[E0000]: division by zero
  --> 1 / 0
          ^
```

### `E0001` — Parse Error

Syntax errors during math expression parsing.

```
error[E0001]: unexpected token
  --> 2 + + 3
          ^
```

### `E0002` — Function Domain Error

A math function received an argument outside its valid domain.

```
error[E0002]: sqrt() requires non-negative argument
  --> sqrt(-1)
      ^^^^^^^
```

Also used for:
- **Unknown command** — An unrecognized colon command was entered
- **Unknown subcommand** — A valid command received an unknown subcommand

### `E0003` — Trailing Flag

An unexpected flag was found after a command.

```
error[E0003]: unexpected trailing flag
  --> :var set x = 5 --unknown
                     ^^^^^^^^^
```

---

## Solver Errors (`E0300–E0399`)

### `E0301` — No Solution

The equation has no solution.

```
error[E0301]: equation has no solution
  --> 0x = 5
      ^^^^^^
  help: the equation simplifies to 0 = 5 which is a contradiction
```

### `E0302` — Infinite Solutions

The equation is satisfied for all values of the variable.

```
error[E0302]: infinitely many solutions
  --> x = x
      ^^^^^
  help: the equation simplifies to 0 = 0 (identity)
```

### `E0303` — Invalid Equation

The input is not a valid equation.

```
error[E0303]: invalid equation
  --> not an equation
```

### `E0304` — Multiple Unknowns

A single equation contains more than one unknown variable.

```
error[E0304]: equation has multiple unknowns
  --> x + y = 5
      ^^^^^^^^^
  help: a single equation can only be solved for one variable
```

### `E0310` — System/Polynomial Error

Covers multiple related cases:
- **System has no solution** — A matrix system produced no solution
- **Polynomial solver error** — The polynomial solver encountered a problem

### `E0311` — System Infinite Solutions

A system of equations has infinitely many solutions (underdetermined).

### `E0315` — Unsupported Equation

The equation type is not supported by the current solver.

```
error[E0315]: unsupported equation type
  help: only linear and polynomial equations are supported
```

---

## Variable Errors (`E0400–E0499`)

### `E0391` — Circular Dependency

Variables reference each other in a cycle.

```
error[E0391]: circular dependency detected
  --> a
      ^
  help: a → b → a creates a cycle
```

### `E0401` — Missing Variable Name

A `:var` command was used without specifying a name.

```
error[E0401]: missing variable name
  --> :var set
              ^
  help: usage: :var set <name> = <expr>
```

### `E0402` — Reserved Keyword

Attempted to use a reserved name as a variable.

```
error[E0402]: cannot use reserved keyword as variable name
  --> :var set pi = 5
               ^^
  help: 'pi' is a built-in constant
```

### `E0403` — Invalid Identifier

The variable name is not a valid identifier.

```
error[E0403]: invalid variable name
  --> :var set 123abc = 5
               ^^^^^^
```

### `E0404` — Missing Expression

A variable assignment is missing the right-hand side expression.

```
error[E0404]: missing expression after '='
  --> :var set x =
                   ^
```

### `E0405` — Multiple Assignment Not Supported

Attempted to assign multiple variables in a single command.

### `E0425` — Undefined Variable

A variable was referenced but never defined.

```
error[E0425]: undefined variable 'z'
  --> z + 1
      ^
  help: use :var set z = <value> to define it
```

---

## Config/Settings Errors (`E0500–E0599`)

### `E0502` — Missing Setting Key

A `:config` command was used without specifying a key (or key-value pair).

```
error[E0502]: missing setting key
  --> :config set
                  ^
  help: usage: :config set <key> <value>
```

### `E0503` — Unknown Setting

The specified setting key doesn't exist.

```
error[E0503]: unknown setting 'output.color'
  --> :config set output.color true
                  ^^^^^^^^^^^^
  help: run :config show to see available settings
```

### `E0504` — Invalid Setting Value

The value provided is not valid for the setting's type.

```
error[E0504]: invalid value for 'output.decimals'
  --> :config set output.decimals abc
                                  ^^^
  help: expected an integer
```

### `E0505` — Environment Reference Error

A setting that references an environment encountered a problem.

---

## Environment Errors (`E0600–E0699`)

### `E0600` — Missing Environment Name

An `:env` command was used without specifying an environment name.

```
error[E0600]: missing environment name
  --> :env switch
                  ^
  help: usage: :env switch <name>
```

### `E0601` — Environment Not Found

The specified environment doesn't exist.

```
error[E0601]: environment 'work' not found
  --> :env switch work
                  ^^^^
  help: use :env new <name> to create it, or :env ls to list available
```

---

## History Errors (`E0700–E0799`)

### `E0701` — History Range Error

The specified history range is invalid.

```
error[E0701]: history range out of bounds
  --> :redo 999
            ^^^
```

### `E0702` — History Missing Argument

A `:redo` command was used without a required argument.

### `E0703` — History Write Error

Unable to persist history to disk.

---

## Runtime Errors (`E0800–E0899`)

Runtime errors are generated by `src/diagnostics/kinds/runtime_kind.hpp` and `src/diagnostics/kinds/runtime_extra_kind.hpp`. These cover infrastructure-level problems such as file I/O failures.

---

## Polynomial Errors

Polynomial-specific errors are defined in `src/diagnostics/kinds/polynomial_kind.hpp` and may overlap with solver error codes (`E0310`, `E0315`).

---

## Error Anatomy

Every `Diagnostic` carries the following information:

```
error[E0425]: undefined variable 'z'         ← level + code + message
  --> z + 1                                   ← source text
      ^                                       ← inline label (span)
  help: use :var set z = <value> to define it ← help text
```

| Field     | Type                         | Purpose                               |
| --------- | ---------------------------- | ------------------------------------- |
| `level`   | `DiagLevel`                  | `Error` or `Warning`                  |
| `code`    | `std::string`                | Error code (e.g., `"E0425"`)          |
| `message` | `std::string`                | Human-readable message                |
| `span`    | `Span`                       | Source location (byte offsets)        |
| `label`   | `std::string`                | Inline label shown under the caret(s) |
| `help`    | `std::optional<std::string>` | Additional help text                  |
| `input`   | `std::string`                | Original source text for rendering    |

---

## Adding New Error Codes

See the contributing guide: [Adding a Command](../contributing/adding-a-command.md#step-7-add-error-kinds-if-needed) for the pattern used to define new error codes via factory functions.
