# cmath-solver Documentation

Welcome to the **cmath-solver** developer documentation. This guide covers architecture, internals, and contribution workflows for the project.

## Quick Links

| Document                                               | Description                                                                        |
| ------------------------------------------------------ | ---------------------------------------------------------------------------------- |
| [Architecture Overview](architecture/overview.md)      | High-level pipeline, execution modes, build system, design principles              |
| [AST](architecture/ast.md)                             | Node hierarchies, visitor pattern, ownership model, tree examples                  |
| [Parser](architecture/parser.md)                       | Lexer tokens, recursive descent, precedence climbing, command subparsers           |
| [Evaluation](architecture/evaluation.md)               | Evaluator, Resolver, lazy evaluation, cycle detection, broadcast                   |
| [Algebra](architecture/algebra/README.md)              | Linear forms, polynomials, factoring, solvers, matrix systems (detailed sub-pages) |
| [Commands](architecture/commands.md)                   | Registry dispatch, handlers, history tracking, REPL integration                    |
| [Diagnostics](architecture/diagnostics.md)             | `Result<T>`, `Diagnostic`, error rendering, error propagation                      |
| [Config & Runtime](architecture/config-and-runtime.md) | Settings, environments, context, REPL, script runner                               |

## Contributing

| Document                                                         | Description                                                |
| ---------------------------------------------------------------- | ---------------------------------------------------------- |
| [Contributing Guide](contributing/CONTRIBUTING.md)               | Prerequisites, workflow, code style, PR process            |
| [Adding a Command](contributing/adding-a-command.md)             | Step-by-step: AST → visitor → subparser → handler → tests  |
| [Adding an AST Node](contributing/adding-an-ast-node.md)         | Step-by-step: node → visitor → all implementations → tests |
| [Adding a Math Function](contributing/adding-a-math-function.md) | Step-by-step: `FuncKind` → hash map → resolver → tests     |
| [Testing Guide](contributing/testing.md)                         | GoogleTest, UI tests, `--bless` workflow                   |

## Reference

| Document                                | Description                                    |
| --------------------------------------- | ---------------------------------------------- |
| [Error Codes](reference/error-codes.md) | Complete E0000–E0900 table by subsystem        |
| [Settings](reference/settings.md)       | All config keys, types, defaults, valid values |

## Project Overview

**cmath-solver** is a C++17 command-line calculator featuring:

- Interactive REPL with tab-completion, syntax highlighting, and history
- Symbolic variable storage with lazy evaluation
- Linear & polynomial equation solving (single and systems)
- Polynomial expansion and factoring
- 17 built-in math functions
- Named environments for workspace isolation
- Script execution with rollback support
- Rich diagnostic error messages with source spans

See the [README](../README.md) for user-facing documentation and the [INSTALL guide](../INSTALL.md) for build setup.
