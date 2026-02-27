
# Math Solver

An interactive command-line math tool built in C++17. It can evaluate arithmetic expressions, store variables (including symbolic expressions), solve linear equations, simplify equations to canonical form, expand polynomial expressions, and factor polynomials — all from a REPL with history, completions, and color output.

**Version**: 1.1.1

---

## Installation

See [INSTALL.md](INSTALL.md) for platform-specific setup instructions (Windows and Linux).

---

## Quick Start

```bash
cmake -S . -B build -G Ninja -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
ninja -C build
```

Run the program:

```bash
# Windows
.\build\bin\math-solver.exe

# Linux
./build/bin/math-solver
```

You can also evaluate a single expression directly from the command line:

```bash
./build/bin/math-solver "2 + 3 * 4"
```

---

## How It Works

### Overview

Math Solver is a sophisticated REPL (Read-Eval-Print Loop) application with a modular, layered architecture. The user inputs either a math expression or a command, which is processed through a comprehensive pipeline:

```
Input string
  |
  v
[ Input Router ] --- detect if input is math expression or command
  |
  v
[ Lexer ] --- tokenize characters into tokens (numbers, operators, identifiers)
  |                     (math lexer or command lexer)
  v
[ Parser ] --- build an Abstract Syntax Tree (AST) from tokens
  |                     (math parser or command parser)
  v
[ Command Registry ] --- dispatch to appropriate handler
  |
  v
[ Evaluator / Solver / Simplifier / Polynomial ] --- compute, solve, simplify, expand, or factor
  |
  v
[ Formatters ] --- format output with colors and proper styling
  |
  v
Output
```

### Architecture Layers

#### 1. Input Processing Layer
- **Input Router** (`parser/input_router.hpp`) — Analyzes raw input to determine if it's a math expression or command
- **Lexers** (`lexer/`) — Two specialized lexers:
  - Math lexer: tokenizes mathematical expressions (`Number`, `Identifier`, `Plus`, `Minus`, `Mul`, `Div`, `Pow`, `LParen`, `RParen`, `Equals`)
  - Command lexer: tokenizes REPL commands (`Colon`, `Identifier`, `String`, etc.)

#### 2. Parsing Layer
- **Math Parser** (`parser/math/math_parser.cpp`) — Recursive descent parser with precedence climbing for mathematical expressions
- **Command Parser** (`parser/command/`) — Modular command parsing system with specialized subparsers for each command type
- **Parser Registry** — Dynamic registration system for command parsers

#### 3. AST Layer
- **Math Expressions** (`ast/math/`) — Expression tree nodes:
  - `NumberExpr` — numeric literals (e.g. `42`, `3.14`)
  - `VariableExpr` — variable references (e.g. `x`, `mass`)
  - `BinaryExpr` — binary operations (`+`, `-`, `*`, `/`, `^`)
  - `EquationExpr` — equations (`lhs = rhs`)
- **Commands** (`ast/command/`) — Command AST nodes for different command types

#### 4. Command Execution Layer
- **Command Registry** (`commands/registry.cpp`) — Central command dispatch system
- **Command Handlers** (`commands/handlers/`) — Specialized handlers for each command category
- **Runtime System** (`runtime/`) — Manages execution context, variable resolution, and validation

#### 5. Mathematical Processing Layer
- **Evaluator** (`eval/evaluator.cpp`) — Expression evaluation using Visitor pattern with lazy variable resolution
- **Linear Algebra** (`algebra/linear/`) — Linear equation solving and simplification
- **Polynomial Engine** (`algebra/polynomial/`) — Symbolic polynomial operations (expand, factor)
- **Solver** (`algebra/solver/`) — Equation solving algorithms

#### 6. Context Management
- **Context** (`runtime/context/context.hpp`) — Thread-safe variable storage with circular dependency detection
- **Resolver** (`runtime/context/resolver.cpp`) — Variable resolution with lazy evaluation
- **Validator** (`runtime/context/validator.cpp`) — Context validation and integrity checks

#### 7. Configuration & Persistence
- **Config System** (`config/`) — JSON-based configuration with environment management
- **Environment Support** — Named variable sets that can be saved/loaded
- **Settings Management** — Persistent user preferences

#### 8. User Interface Layer
- **REPL Engine** (`ui/repl/`) — Interactive loop with history, completions, and syntax highlighting
- **Formatters** (`ui/formatters/`) — Output formatting with color support and fraction display
- **Diagnostics** (`diagnostics/`) — Comprehensive error reporting with source spans

### Key Features

#### Variable System
- **Symbolic Storage**: Variables store expressions, not just values
- **Lazy Evaluation**: Expressions evaluated only when needed
- **Circular Dependency Detection**: Prevents infinite recursion
- **Context Isolation**: CLI mode uses temporary context, REPL uses persistent context

#### Error Handling
- **Source Spans**: All errors include precise location information
- **Typed Errors**: Specialized error types for different subsystems
- **Graceful Degradation**: Partial failures don't crash the system
- **User-Friendly Messages**: Clear, actionable error descriptions

#### Extensibility
- **Plugin Architecture**: New commands can be registered dynamically
- **Modular Parsers**: Command-specific parsers can be added independently
- **Visitor Pattern**: Easy to add new AST operations
- **Registry Pattern**: Decoupled component discovery and initialization

### Context and Variables

The `Context` class (`runtime/context/context.hpp`) stores variable bindings as symbolic expressions (`unordered_map<string, ExprPtr>`). Variables can hold:

- **Numeric values**: `:set x 5` stores `NumberExpr(5)`
- **Symbolic expressions**: `:set a x + 2` stores `BinaryExpr(VariableExpr("x"), NumberExpr(2), Add)`
- **Composed expressions**: `:set b a * 3` references `a`, which references `x`

**Lazy evaluation**: Expressions are only evaluated to numeric values when needed. Setting `a = x + 2` and then later setting `x = 5` means `a` evaluates to `7` — variable bindings are always resolved at evaluation time.

**Circular dependency detection**: If `a` references `b` and `b` references `a`, evaluating either will throw a `CircularDependencyError`.

Variables can be set via `:set`, solved equations are automatically stored, and the context persists across the session.

### Environments

Environments allow saving and loading named sets of variables. They are serialized to a JSON config file stored in:

| Platform | Path                     |
| -------- | ------------------------ |
| Windows  | `%APPDATA%\math-solver\` |
| Linux    | `~/.config/math-solver/` |

The environment system supports:
- **Named Environments**: Save/load variable sets with custom names
- **Auto-load**: Automatically load a specified environment on startup
- **Partial Recovery**: Graceful handling of corrupted environment entries
- **Legacy Compatibility**: Fallback to numeric parsing for old environment formats

---

## REPL Commands

| Command                     | Description                               |
| --------------------------- | ----------------------------------------- |
| `<expression>`              | Evaluate an expression (e.g. `2 + 3 * x`) |
| `<lhs> = <rhs>`             | Check if equation is true or false        |
| `solve <lhs> = <rhs>`       | Solve a linear equation for the unknown   |
| `simplify <eq> [flags]`     | Simplify equation to canonical form       |
| `expand <expression>`       | Expand to canonical polynomial form       |
| `factor <polynomial>`       | Factor a polynomial expression            |
| `:set <var> <value>`        | Set a variable to a value or expression   |
| `:set <var> solve <eq>`     | Solve and store the result in a variable  |
| `:unset <var>`              | Remove a variable                         |
| `:ls`                       | List all defined variables                |
| `:clear`                    | Clear all variables                       |
| `:config list`              | Show all settings                         |
| `:config get <key>`         | Get a setting value                       |
| `:config set <key> <value>` | Change a setting                          |
| `:config reset`             | Reset settings to defaults                |
| `:env list | ls`            | List saved environments                   |
| `:env save <name>`          | Save current variables as an environment  |
| `:env load <name>`          | Load an environment                       |
| `:env new <name>`           | Create a new empty environment            |
| `:env delete <name>`        | Delete an environment                     |
| `:help`                     | Show help                                 |
| `:exit` / `:quit` / `:q`    | Exit the program                          |

### Simplify Flags

| Flag         | Effect                            |
| ------------ | --------------------------------- |
| `--vars x y` | Set variable display order        |
| `--isolated` | Ignore context variables          |
| `--fraction` | Display coefficients as fractions |

### Settings

| Key             | Default     | Description                     |
| --------------- | ----------- | ------------------------------- |
| `precision`     | `6`         | Decimal places in output (0–15) |
| `fraction_mode` | `false`     | Default fraction display        |
| `history_size`  | `1000`      | Max REPL history entries        |
| `auto_load_env` | `"default"` | Environment loaded on startup   |

---

## Project Structure

```
cmath-solve/
├── src/
│   ├── main.cpp                              # Entry point, CLI/REPL dispatch
│   ├── ast/                                  # Abstract Syntax Tree definitions
│   │   ├── math/                             #   Math expression nodes
│   │   │   ├── expr.hpp                      #     Base Expr class + Visitor interface
│   │   │   ├── number_expr.hpp               #     Numeric literal node
│   │   │   ├── variable_expr.hpp             #     Variable reference node
│   │   │   ├── binary_expr.hpp               #     Binary operation node (+, -, *, /, ^)
│   │   │   └── equation_expr.hpp             #     Equation node (lhs = rhs)
│   │   └── command/                          #   Command AST nodes
│   │       ├── command.hpp                   #     Base command interface
│   │       ├── math_command.hpp              #     Math evaluation commands
│   │       ├── config_command.hpp            #     Configuration commands
│   │       ├── env_command.hpp               #     Environment commands
│   │       ├── history_command.hpp           #     History management
│   │       ├── load_command.hpp              #     File loading
│   │       ├── redo_command.hpp              #     Command redo
│   │       ├── system_command.hpp            #     System commands
│   │       └── var_command.hpp               #     Variable management
│   ├── lexer/                                # Tokenization layer
│   │   ├── math/                             #   Math expression lexer
│   │   │   ├── math_lexer.hpp                #     Math token definitions
│   │   │   └── math_token.hpp                #     Token types (Number, Identifier, etc.)
│   │   └── command/                          #   Command lexer
│   │       ├── command_lexer.hpp             #     Command token definitions
│   │       ├── command_token.hpp             #     Command token types
│   │       ├── command_token_stream.cpp      #     Token stream management
│   │       └── token_stream.hpp              #     Stream interface
│   ├── parser/                               # Parsing layer
│   │   ├── math/                             #   Math expression parser
│   │   │   ├── math_parser.cpp               #     Recursive descent parser
│   │   │   └── math_parser.hpp               #     Parser class definition
│   │   ├── command/                          #   Command parser
│   │   │   ├── command_parser.cpp            #     Main command parser
│   │   │   ├── command_parser.hpp            #     Parser interface
│   │   │   ├── command_parser_registry.cpp   #     Parser registration system
│   │   │   ├── command_parser_registry.hpp   #     Registry interface
│   │   │   └── subparsers/                   #     Specialized command parsers
│   │   │       ├── config_command_parser.cpp #     Configuration command parsing
│   │   │       ├── env_command_parser.cpp    #     Environment command parsing
│   │   │       ├── history_command_parser.cpp #     History command parsing
│   │   │       ├── load_command_parser.cpp   #     Load command parsing
│   │   │       ├── math_command_parser.cpp   #     Math command parsing
│   │   │       ├── redo_command_parser.cpp   #     Redo command parsing
│   │   │       ├── system_command_parser.cpp #     System command parsing
│   │   │       ├── var_command_parser.cpp    #     Variable command parsing
│   │   │       └── icommand_subparser.hpp    #     Subparser interface
│   │   └── input_router.hpp                  #   Input type detection and routing
│   ├── eval/                                 # Expression evaluation
│   │   ├── evaluator.cpp                    #   Main evaluation engine
│   │   ├── evaluator.hpp                    #   Evaluator class (Visitor pattern)
│   │   └── expander.hpp                      #   Symbolic expression expander
│   ├── algebra/                              # Mathematical operations
│   │   ├── linear/                           #   Linear algebra operations
│   │   │   ├── linear_collector.cpp          #     Collect linear coefficients
│   │   │   ├── linear_collector.hpp          #     Linear form data structures
│   │   │   ├── simplify.cpp                  #     Equation simplification
│   │   │   └── simplify.hpp                  #     Simplification algorithms
│   │   ├── polynomial/                       #   Polynomial operations
│   │   │   ├── ast_to_poly.cpp               #     AST to polynomial conversion
│   │   │   ├── ast_to_poly.hpp               #     Conversion interface
│   │   │   ├── factor.cpp                    #     Polynomial factorization
│   │   │   ├── factor.hpp                    #     Factoring algorithms
│   │   │   ├── monomial.hpp                  #     Monomial representation
│   │   │   └── polynomial.hpp                #     Polynomial data structures
│   │   └── solver/                           #   Equation solving
│   │       ├── solver.cpp                    #     Linear equation solver
│   │       └── solver.hpp                    #     Solver interface
│   ├── runtime/                              # Runtime system
│   │   ├── context/                          #   Variable management
│   │   │   ├── context.cpp                   #     Context implementation
│   │   │   ├── context.hpp                   #     Variable storage (name → ExprPtr)
│   │   │   ├── resolver.cpp                  #     Variable resolution
│   │   │   ├── resolver.hpp                  #     Resolution algorithms
│   │   │   ├── validator.cpp                 #     Context validation
│   │   │   └── validator.hpp                 #     Validation logic
│   │   ├── runtime.cpp                       #   Runtime initialization
│   │   └── runtime.hpp                       #   Runtime interface
│   ├── commands/                             # Command execution system
│   │   ├── handlers/                         #   Command handlers
│   │   │   ├── config_handler.hpp            #     Configuration command handling
│   │   │   ├── env_handler.hpp               #     Environment command handling
│   │   │   ├── history_handler.hpp           #     History command handling
│   │   │   ├── load_handler.hpp              #     Load command handling
│   │   │   ├── math_handler.hpp              #     Math command handling
│   │   │   ├── redo_handler.hpp              #     Redo command handling
│   │   │   ├── system_handler.hpp            #     System command handling
│   │   │   └── var_handler.hpp               #     Variable command handling
│   │   ├── registry.cpp                      #   Command registration
│   │   └── registry.hpp                      #   Registry interface
│   ├── config/                               # Configuration management
│   │   ├── config.cpp                        #   Configuration implementation
│   │   ├── config.hpp                        #   Configuration interface
│   │   ├── environment.hpp                   #   Environment data structures
│   │   └── settings.hpp                      #   Settings definitions
│   ├── diagnostics/                          # Error handling and reporting
│   │   ├── diagnostic.cpp                    #   Diagnostic implementation
│   │   ├── diagnostic.hpp                    #   Diagnostic interface
│   │   ├── result.hpp                        #   Result type for error handling
│   │   ├── sink.hpp                          #   Error output management
│   │   └── kinds/                            #   Error type definitions
│   │       ├── command_errors.hpp            #     Command-related errors
│   │       ├── command_kind.hpp              #     Command error categories
│   │       ├── config_errors.hpp             #     Configuration errors
│   │       ├── env_errors.hpp                #     Environment errors
│   │       ├── history_errors.hpp             #     History errors
│   │       ├── math_errors.hpp               #     Math evaluation errors
│   │       ├── polynomial_errors.hpp         #     Polynomial operation errors
│   │       ├── runtime_errors.hpp            #     Runtime system errors
│   │       ├── runtime_errors_extra.hpp      #     Additional runtime errors
│   │       ├── solver_errors.hpp              #     Solver errors
│   │       └── var_errors.hpp                #     Variable management errors
│   ├── ui/                                   # User interface components
│   │   ├── color.hpp                         #   ANSI color codes
│   │   ├── formatters/                       #   Output formatting
│   │   │   ├── number_formatter.hpp          #     Number formatting utilities
│   │   │   └── output_formatter.hpp          #     General output formatting
│   │   ├── repl/                             #   REPL implementation
│   │   │   ├── completions.hpp               #     Auto-completion system
│   │   │   ├── highlighter.hpp               #     Syntax highlighting
│   │   │   ├── hints.hpp                     #     Input hints
│   │   │   ├── history.hpp                   #     Command history management
│   │   │   ├── repl.cpp                      #     Main REPL loop
│   │   │   ├── repl.hpp                      #     REPL interface
│   │   │   ├── runner.cpp                    #     Command execution runner
│   │   │   └── runner.hpp                    #     Runner interface
│   │   └── suggestions.hpp                   #   Fuzzy suggestion system
│   ├── core/                                 # Core utilities
│   │   ├── fraction.hpp                      #   Fraction arithmetic and formatting
│   │   └── span.hpp                          #   Source position tracking
│   └── utils/                                # General utilities
│       ├── history_range.hpp                 #   History range parsing
│       ├── path_utils.hpp                    #   File path utilities
│       ├── string_utils.hpp                  #   String manipulation utilities
│       └── utils.hpp                         #   General utility functions
├── tests/                                    # Test suite
├── CMakeLists.txt                            # Build configuration
├── INSTALL.md                                # Installation guide
├── README.md                                 # This file
├── error.msl                                # Error example file
└── x_push_y.msl                             # Push operation example
```

---

## Build System

The project uses CMake with a two-tier architecture:

### Core Library (`math_core`)
- Contains all the main functionality except the CLI entry point
- Designed as a reusable library for potential integration
- Includes all AST, parsing, evaluation, and mathematical operations
- Links against external dependencies (nlohmann/json, replxx)

### Executable (`math-solver`)
- Minimal CLI wrapper around the core library
- Handles command-line argument parsing and REPL initialization
- Links against `math_core` and `replxx` directly for CLI features

### Dependency Management
- Uses CMake `FetchContent` for automatic dependency fetching
- No manual dependency installation required
- Dependencies are built from source and linked statically
- Supports both MSVC and GCC/Clang compilers

### Build Features
- **Ninja Generator**: Optimized for fast parallel builds
- **Compile Commands**: Generates `compile_commands.json` for clangd
- **Warning Suppression**: Third-party dependencies don't generate warnings
- **Cross-Platform**: Works on Windows (MSVC) and Linux (GCC/Clang)

---

## Dependencies

| Library                                            | Version | Purpose               |
| -------------------------------------------------- | ------- | --------------------- |
| [nlohmann/json](https://github.com/nlohmann/json)  | v3.11.3 | JSON config storage   |
| [replxx](https://github.com/AmokHuginnsson/replxx) | v0.0.4  | Interactive CLI input |

Dependencies are fetched automatically via CMake `FetchContent` during configuration. No manual installation needed.

---

## Documentation

Detailed implementation walkthroughs for each feature are available in the [`docs/`](docs/) folder:

| Feature            | Documentation                                            |
| ------------------ | -------------------------------------------------------- |
| Dispatch Overview  | [`docs/dispatch_overview.md`](docs/dispatch_overview.md) |
| `evaluate`         | [`docs/evaluate_command.md`](docs/evaluate_command.md)   |
| `solve`            | [`docs/solve_command.md`](docs/solve_command.md)         |
| `simplify`         | [`docs/simplify_command.md`](docs/simplify_command.md)   |
| `expand`           | [`docs/expand_command.md`](docs/expand_command.md)       |
| `factor`           | [`docs/factor_command.md`](docs/factor_command.md)       |
| `:set` / `:unset`  | [`docs/set_command.md`](docs/set_command.md)             |
| `:vars` / `:clear` | [`docs/variable_commands.md`](docs/variable_commands.md) |
| `:config`          | [`docs/config_command.md`](docs/config_command.md)       |
| `:env`             | [`docs/env_command.md`](docs/env_command.md)             |