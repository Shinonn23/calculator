# `:set` Command — Walkthrough แบบละเอียด

## สารบัญ
1. [ภาพรวม (Overview)](#1-ภาพรวม)
2. [Flow A: `:set x 42` — กำหนดค่าจาก expression](#flow-a--set-ค่าจาก-expression)
3. [Flow B: `:set x solve 2x + 3 = 7` — solve แล้วเก็บ](#flow-b--set-ค่าจาก-solve)
4. [การ Validate ชื่อตัวแปร](#การ-validate-ชื่อตัวแปร)
5. [Error Cases](#error-cases)

---

## 1. ภาพรวม

`:set` มี 2 โหมด:

| โหมด | ตัวอย่าง | สิ่งที่เกิดขึ้น |
|-------|----------|----------------|
| **กำหนดค่า** | `:set x 42` | parse `42` → evaluate → เก็บลง context |
| **กำหนดค่า (expression)** | `:set x 2+3` | parse `2+3` → evaluate = 5 → เก็บลง context |
| **solve แล้วเก็บ** | `:set x solve 2x+3=7` | solve สมการ → ได้ x=2 → เก็บลง context ในชื่อ `x` |

```
User Input: ":set x 42"
  → dispatch() ตรวจ starts_with(":set ") → true
  → cmd_set("x 42", g_ctx, g_config)
```

---

## Flow A — `:set` ค่าจาก expression

**Input:** `:set radius 2 + 3`

### Step 1 — แยก ชื่อตัวแปร กับ value

```cpp
// command.hpp — cmd_set()
vector<string> parts = split(args);     // args = "radius 2 + 3"
                                         // parts = ["radius", "2", "+", "3"]
if (parts.size() < 2) { /* Usage error */ }

string var_name = parts[0];              // var_name = "radius"

// ประกอบ value string จาก parts[1] เป็นต้นไป
string value_str;
for (size_t i = 1; i < parts.size(); ++i) {
    if (i > 1) value_str += " ";
    value_str += parts[i];
}
// value_str = "2 + 3"
```

### Step 2 — Validate ชื่อตัวแปร

```cpp
// ตรวจตัวอักษรแรก: ต้องเป็น letter หรือ _
if (var_name.empty() || !(isalpha(var_name[0]) || var_name[0] == '_')) {
    // Error: invalid variable name
}

// ตรวจทุกตัวอักษร: ต้องเป็น alnum หรือ _
for (char c : var_name) {
    if (!isalnum(c) && c != '_') { /* Error */ }
}

// ตรวจว่าไม่ใช่ reserved keyword (solve, simplify, exit, quit ฯลฯ)
if (is_reserved_keyword(var_name)) { /* Error */ }
```

**ตัวอย่างชื่อที่ผ่าน/ไม่ผ่าน:**

| ชื่อ | ผ่าน? | เหตุผล |
|------|-------|--------|
| `radius` | ✅ | ตัวอักษรทั้งหมด |
| `x1` | ✅ | เริ่มด้วย letter + ตัวเลข |
| `_temp` | ✅ | เริ่มด้วย `_` |
| `2x` | ❌ | เริ่มด้วยตัวเลข |
| `my-var` | ❌ | มี `-` (ไม่ใช่ alnum/underscore) |
| `solve` | ❌ | reserved keyword |

### Step 3 — ตรวจว่าไม่ได้ขึ้นต้นด้วย `solve`

```cpp
if (starts_with(value_str, "solve ")) {
    // → ไปทำ Flow B แทน
}
```

`"2 + 3"` ไม่ขึ้นต้นด้วย `"solve "` → ทำต่อเป็น expression evaluate

### Step 4 — Parse & Evaluate

```cpp
Parser    parser(value_str);          // value_str = "2 + 3"
auto      expr = parser.parse();     // AST: BinaryOp(Add, Number(2), Number(3))
Evaluator eval(&g_ctx, value_str);
double    value = eval.evaluate(*expr);
// visit BinaryOp(Add)
//   visit Number(2) → 2
//   visit Number(3) → 3
//   Add: 2 + 3 = 5
// value = 5

g_ctx.set(var_name, value);           // g_ctx.set("radius", 5)
// ⚡ ตอนนี้ g_ctx = {..., radius: 5}

cout << "  " << var_name << " = " << fmt(value, g_config) << "\n";
// Output: "  radius = 5"
```

---

## Flow B — `:set` ค่าจาก solve

**Input:** `:set ans solve 2x + 3 = 7`

### Step 1-2 — เหมือน Flow A (แยกชื่อ, validate)

```
var_name  = "ans"
value_str = "solve 2x + 3 = 7"
```

### Step 3 — ตรวจพบ `solve` prefix

```cpp
if (starts_with(value_str, "solve ")) {
    string eq_str = value_str.substr(6);    // eq_str = "2x + 3 = 7"
    // ...
}
```

### Step 4 — สร้าง `temp_ctx` ที่ไม่มี `ans`

```cpp
Parser  parser(eq_str);
auto    eq = parser.parse_equation();     // AST ของ "2x + 3 = 7"

// สร้าง context ที่ exclude var_name ("ans") เพื่อป้องกัน shadowing
Context temp_ctx;
for (const auto& [name, value] : g_ctx.all()) {
    if (name != var_name)                 // ทุกตัวยกเว้น "ans"
        temp_ctx.set(name, value);
}
```

> สังเกต: ต่างจาก `cmd_solve()` ที่ exclude ตัวแปร**ในสมการ** — `:set` จะ exclude ตัวแปร**ที่จะเก็บค่า** (var_name) แทน

### Step 5 — Solve ด้วย EquationSolver

```cpp
EquationSolver solver(&temp_ctx, eq_str);
SolveResult    result = solver.solve(*eq);
// (flow ภายในเหมือน solve_command.md ทุกประการ)
// result = { variable: "x", value: 2.0, has_solution: true }
```

### Step 6 — เก็บค่าในชื่อ `ans` (ไม่ใช่ `x`!)

```cpp
if (result.has_solution) {
    g_ctx.set(var_name, result.value);    // g_ctx.set("ans", 2.0)
    //        ^^^^^^^^
    //        ใช้ var_name ไม่ใช่ result.variable!
    //        ⚡ ดังนั้น: g_ctx = {..., ans: 2}

    cout << "  " << var_name << " = " << fmt(result.value, g_config) << "\n";
    // Output: "  ans = 2"
}
```

> **ข้อแตกต่างจาก `solve` command:**
> | | `solve 2x+3=7` | `:set ans solve 2x+3=7` |
> |---|---|---|
> | ตัวแปรที่เก็บ | `x` (จากสมการ) | `ans` (ที่ user ระบุ) |
> | แสดง `(saved)` | ✅ | ❌ |
> | Output | `x = 2  (saved)` | `ans = 2` |

---

## การ Validate ชื่อตัวแปร

Flow ทั้งหมดของการ validate:

```
":set 2x 10"
  │
  ├── ตัวอักษรแรก: '2' → isalpha? ❌ → Error: invalid variable name '2x'
  │
":set my-var 10"
  │
  ├── ตัวอักษรแรก: 'm' → isalpha? ✅
  ├── loop ตรวจทุกตัว: 'm','y','-',...
  │                          ↑ isalnum('-')? ❌, '_'? ❌
  └── Error: invalid variable name 'my-var'

":set solve 10"
  │
  ├── ตัวอักษรแรก: 's' → ✅
  ├── ทุกตัวอักษร: ผ่าน ✅
  └── is_reserved_keyword("solve")? ✅ → Error: 'solve' is a reserved keyword
```

---

## Error Cases

### `:set` โดยไม่ให้ค่า

```
Input: ":set x"
```
- `parts = ["x"]` → `parts.size() < 2` → Usage error
- Output: `  Usage: :set <variable> <value>`

### `:set` ค่าจาก expression ที่มี undefined variable

```
Input: ":set y a + 5"    (g_ctx ไม่มี a)
```
- parse `"a + 5"` → evaluate → visit Variable("a") → throw `UndefinedVariableError`
- Output: error message

### `:set` solve สมการที่มีหลายตัวแปร

```
Input: ":set ans solve x + y = 10"    (g_ctx ไม่มี x, y)
```
- solver เจอ 2 unknowns → throw `MultipleUnknownsError`
- Output: error + Hint: `use :set to define variables, or use simplify`

### `:set` solve สมการที่ไม่มีคำตอบ

```
Input: ":set ans solve 5 = 3"
```
- solver เจอ constant equation → throw `NoSolutionError`
- Output: `  Error: equation has no solution`
