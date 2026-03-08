# Settings Reference

> **Complete reference of all configurable settings in cmath-solver.**
>
> Settings are managed with the `:config` command and persisted in JSON at:
> - **Linux/macOS:** `~/.config/cmath-solver/settings.json`
> - **Windows:** `%APPDATA%\cmath-solver\settings.json`

## Quick Commands

```
:config show              # Show all settings and their values
:config show <key>        # Show a single setting
:config set <key> <value> # Change a setting
:config reset             # Reset all settings to defaults
```

---

## Output Settings

Settings that control how results are displayed.

### `output.mode`

|                 |                                                                                                                                                             |
| --------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Type**        | `string`                                                                                                                                                    |
| **Default**     | `"auto"`                                                                                                                                                    |
| **Values**      | `"auto"`, `"decimal"`, `"fraction"`                                                                                                                         |
| **Description** | Controls the output format for numeric results. `"auto"` chooses the best representation; `"decimal"` forces decimal; `"fraction"` forces fraction display. |

```
:config set output.mode fraction
> 1/3
= 1/3

:config set output.mode decimal
> 1/3
= 0.333333
```

### `output.decimals`

|                 |                                      |
| --------------- | ------------------------------------ |
| **Type**        | `int`                                |
| **Default**     | `6`                                  |
| **Description** | Number of decimal places to display. |

```
:config set output.decimals 2
> pi
= 3.14
```

### `output.fraction`

|                 |                                                                                           |
| --------------- | ----------------------------------------------------------------------------------------- |
| **Type**        | `bool`                                                                                    |
| **Default**     | `false`                                                                                   |
| **Description** | When `true`, prefer fraction output for results that can be expressed as exact fractions. |

### `output.trailing_zeros`

|                 |                                                                         |
| --------------- | ----------------------------------------------------------------------- |
| **Type**        | `bool`                                                                  |
| **Default**     | `false`                                                                 |
| **Description** | When `true`, display trailing zeros up to the configured decimal count. |

```
:config set output.trailing_zeros true
:config set output.decimals 4
> 2.5
= 2.5000
```

### `output.thousands_sep`

|                 |                                                              |
| --------------- | ------------------------------------------------------------ |
| **Type**        | `bool`                                                       |
| **Default**     | `false`                                                      |
| **Description** | When `true`, format large numbers with thousands separators. |

```
:config set output.thousands_sep true
> 1000000
= 1,000,000
```

---

## Solver Settings

Settings that control the equation solver and numerical algorithms.

### `solver.tolerance`

|                 |                                                                                                        |
| --------------- | ------------------------------------------------------------------------------------------------------ |
| **Type**        | `double`                                                                                               |
| **Default**     | `1e-12` (`kEpsilon`)                                                                                   |
| **Description** | General numerical tolerance for floating-point comparisons (e.g., is a coefficient effectively zero?). |

### `solver.coeff_tol`

|                 |                                                                                                                                            |
| --------------- | ------------------------------------------------------------------------------------------------------------------------------------------ |
| **Type**        | `double`                                                                                                                                   |
| **Default**     | `1e-9` (`kCoeffTol`)                                                                                                                       |
| **Description** | Tolerance for coefficient comparison during linear form collection and simplification. Coefficients smaller than this are treated as zero. |

### `solver.pivot_tol`

|                 |                                                                                                                                     |
| --------------- | ----------------------------------------------------------------------------------------------------------------------------------- |
| **Type**        | `double`                                                                                                                            |
| **Default**     | `1e-10` (`kPivotTol`)                                                                                                               |
| **Description** | Pivot tolerance for Gaussian elimination in the matrix solver. Pivots smaller than this trigger row swaps or singularity detection. |

### `solver.max_iter`

|                 |                                                                                         |
| --------------- | --------------------------------------------------------------------------------------- |
| **Type**        | `int`                                                                                   |
| **Default**     | `1000`                                                                                  |
| **Description** | Maximum iterations for iterative solvers (e.g., Durand-Kerner polynomial root finding). |

---

## History Settings

Settings that control REPL command history.

### `history.size`

|                 |                                             |
| --------------- | ------------------------------------------- |
| **Type**        | `int`                                       |
| **Default**     | `1000`                                      |
| **Description** | Maximum number of history entries to store. |

### `history.dedup`

|                 |                                                                      |
| --------------- | -------------------------------------------------------------------- |
| **Type**        | `bool`                                                               |
| **Default**     | `true`                                                               |
| **Description** | When `true`, consecutive duplicate entries are not added to history. |

### `history.ignore`

|                 |                                                        |
| --------------- | ------------------------------------------------------ |
| **Type**        | `string`                                               |
| **Default**     | `""` (empty)                                           |
| **Description** | A pattern or prefix for lines to exclude from history. |

---

## REPL Settings

Settings that control the interactive REPL experience.

### `repl.prompt`

|                 |                                                  |
| --------------- | ------------------------------------------------ |
| **Type**        | `string`                                         |
| **Default**     | `"> "`                                           |
| **Description** | The prompt string displayed in interactive mode. |

```
:config set repl.prompt "calc> "
calc> 2 + 2
= 4
```

### `repl.show_timing`

|                 |                                                             |
| --------------- | ----------------------------------------------------------- |
| **Type**        | `bool`                                                      |
| **Default**     | `false`                                                     |
| **Description** | When `true`, display evaluation time after each expression. |

### `repl.auto_save_env`

|                 |                                                                                       |
| --------------- | ------------------------------------------------------------------------------------- |
| **Type**        | `bool`                                                                                |
| **Default**     | `true`                                                                                |
| **Description** | When `true`, automatically save the active environment to disk when variables change. |

### `repl.confirm_delete`

|                 |                                                                                          |
| --------------- | ---------------------------------------------------------------------------------------- |
| **Type**        | `bool`                                                                                   |
| **Default**     | `true`                                                                                   |
| **Description** | When `true`, prompt for confirmation before deleting environments or clearing variables. |

### `auto_load_env`

|                 |                                                                                           |
| --------------- | ----------------------------------------------------------------------------------------- |
| **Type**        | `string`                                                                                  |
| **Default**     | `""` (empty)                                                                              |
| **Description** | Environment name to automatically load on startup. If empty, starts with a clean context. |

```
:config set auto_load_env work
# Next time cmath-solver starts, the "work" environment is loaded automatically
```

---

## Summary Table

| Key                     | Type   | Default  | Category |
| ----------------------- | ------ | -------- | -------- |
| `output.mode`           | string | `"auto"` | Output   |
| `output.decimals`       | int    | `6`      | Output   |
| `output.fraction`       | bool   | `false`  | Output   |
| `output.trailing_zeros` | bool   | `false`  | Output   |
| `output.thousands_sep`  | bool   | `false`  | Output   |
| `solver.tolerance`      | double | `1e-12`  | Solver   |
| `solver.coeff_tol`      | double | `1e-9`   | Solver   |
| `solver.pivot_tol`      | double | `1e-10`  | Solver   |
| `solver.max_iter`       | int    | `1000`   | Solver   |
| `history.size`          | int    | `1000`   | History  |
| `history.dedup`         | bool   | `true`   | History  |
| `history.ignore`        | string | `""`     | History  |
| `repl.prompt`           | string | `"> "`   | REPL     |
| `repl.show_timing`      | bool   | `false`  | REPL     |
| `repl.auto_save_env`    | bool   | `true`   | REPL     |
| `repl.confirm_delete`   | bool   | `true`   | REPL     |
| `auto_load_env`         | string | `""`     | REPL     |

---

## Configuration File Format

Settings are stored as flat JSON:

```json
{
    "output.mode": "auto",
    "output.decimals": 6,
    "output.fraction": false,
    "output.trailing_zeros": false,
    "output.thousands_sep": false,
    "solver.tolerance": 1e-12,
    "solver.coeff_tol": 1e-9,
    "solver.pivot_tol": 1e-10,
    "solver.max_iter": 1000,
    "history.size": 1000,
    "history.dedup": true,
    "history.ignore": "",
    "repl.prompt": "> ",
    "repl.show_timing": false,
    "repl.auto_save_env": true,
    "repl.confirm_delete": true,
    "auto_load_env": ""
}
```

Only settings that differ from defaults are persisted. Missing keys use default values.
