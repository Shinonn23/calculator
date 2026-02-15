# REPL Dispatch — ภาพรวมการ Route คำสั่ง

## REPL Loop (main.cpp)

```
เปิดโปรแกรม
  → load config → load auto_load_env → g_ctx พร้อมใช้งาน
  → แสดง banner: "Math Solver v1.0 [default]"
  → เข้า REPL loop ↓

┌──────────────────────────────────────────────────────┐
│  [default] >  ← prompt แสดง env ปัจจุบัน             │
│  ผู้ใช้พิมพ์ input                                    │
│  → trim whitespace                                   │
│  → add to history                                    │
│  → dispatch(input)    ← route ไปยัง command ที่ถูกต้อง │
│  → loop กลับมารอ input ใหม่                           │
└──────────────────────────────────────────────────────┘
```

## dispatch() — Routing Table

```cpp
bool dispatch(const string& input) {
    // ตรวจทีละเงื่อนไข — ถ้า match ก็ เรียก command แล้ว return true
}
```

| ลำดับ | เงื่อนไข                                 | Command                    | เอกสาร                                       |
| --- | -------------------------------------- | -------------------------- | -------------------------------------------- |
| 1   | `"exit"` / `"quit"` / `"q"`            | return false (ออกจาก REPL) | —                                            |
| 2   | `":help"` / `":h"`                     | `print_help()`             | —                                            |
| 3   | starts_with `":set "`                  | `cmd_set(args)`            | [set_command.md](set_command.md)             |
| 4   | starts_with `":unset "`                | `cmd_unset(args)`          | [variable_commands.md](variable_commands.md) |
| 5   | `":clear"` / `":cls"`                  | `cmd_clear()`              | [variable_commands.md](variable_commands.md) |
| 6   | `":vars"`                              | `cmd_vars()`               | [variable_commands.md](variable_commands.md) |
| 7   | `":config"` / starts_with `":config "` | `cmd_config(args)`         | [config_command.md](config_command.md)       |
| 8   | `":env"` / starts_with `":env "`       | `cmd_env(args)`            | [env_command.md](env_command.md)             |
| 9   | starts_with `"solve "`                 | `cmd_solve(args)`          | [solve_command.md](solve_command.md)         |
| 10  | starts_with `"simplify "`              | `cmd_simplify(args)`       | [simplify_command.md](simplify_command.md)   |
| 11  | bare command (เช่น `"set x 5"`)         | แนะนำ `:set`                | —                                            |
| 12  | **อื่นๆ ทั้งหมด**                          | `cmd_evaluate(input)`      | [evaluate_command.md](evaluate_command.md)   |

## ตัวอย่าง

```
[default] > 2 + 3          → ไม่ match อะไรเลย → cmd_evaluate("2 + 3")
[default] > solve x+1=5    → match #9 → cmd_solve("x+1=5")
[default] > :set x 10      → match #3 → cmd_set("x 10")
[default] > set x 10       → match #11 → "Did you mean :set?"
[default] > exit            → match #1 → return false → ออก REPL
```

## Cleanup — ก่อนปิดโปรแกรม

```cpp
// main.cpp — หลัง REPL loop จบ
save_current_env_to_config(g_config, g_current_env, g_ctx);  // save ตัวแปรลง config
g_config.save();                                              // เขียน JSON file
rx.history_save(hist_path);                                   // save command history
```
