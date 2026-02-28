# Command Reference Index

This directory contains Detailed Walkthrough documents for every command group in Math Solver.

| File                                     | Commands                          | Summary                                                                              |
| ---------------------------------------- | --------------------------------- | ------------------------------------------------------------------------------------ |
| [math_commands.md](math_commands.md)     | expressions, equations            | Core math pipeline: evaluate expressions, solve equations, expand/factor polynomials |
| [var_command.md](var_command.md)         | `:set`, `:unset`, `:rm`           | Manage named variable bindings in the runtime context                                |
| [config_command.md](config_command.md)   | `:config`, `:conf`                | Read and write runtime settings (output format, solver tolerances, REPL options)     |
| [env_command.md](env_command.md)         | `:env`                            | Manage named environments — save, load, copy, move, and delete variable snapshots    |
| [history_command.md](history_command.md) | `:history`                        | View, search, and export session history                                             |
| [redo_command.md](redo_command.md)       | `:redo`                           | Re-execute one or more commands from session history                                 |
| [load_command.md](load_command.md)       | `:load`                           | Execute a `.msl` script file, with optional rollback on error                        |
| [system_command.md](system_command.md)   | `:exit`, `:help`, `:clear`, `:ls` | REPL lifecycle and utility commands                                                  |

---

## Pipeline overview

All commands (math and colon-prefixed) follow the same dispatch pipeline:

```
Input → InputRouter → Lexer → Parser → AST → CommandRegistry → Handler → Output
```

See [`docs/dispatch_overview.md`](../dispatch_overview.md) for a full walkthrough of the pipeline.

---

## Error code ranges

| Range           | Subsystem                    |
| --------------- | ---------------------------- |
| `E0001`–`E0099` | General / unknown command    |
| `E0100`–`E0199` | Math parsing & evaluation    |
| `E0200`–`E0299` | Variable (`:set` / `:unset`) |
| `E0500`–`E0599` | Config                       |
| `E0600`–`E0699` | Environment (`:env`)         |
| `E0700`–`E0799` | History                      |
| `E0800`–`E0899` | Load / script execution      |
