# `simplify` Command — Walkthrough แบบละเอียด

## สารบัญ
1. [ภาพรวม (Overview)](#1-ภาพรวม)
2. [Step 1 — Parse Flags](#step-1--parse-flags)
3. [Step 2 — Shadow Detection (ตรวจจับตัวแปรที่ซ้อน)](#step-2--shadow-detection)
4. [Step 3 — Collect & Normalize](#step-3--collect--normalize)
5. [Step 4 — Format Canonical](#step-4--format-canonical)
6. [ตัวอย่าง: `--vars`, `--isolated`, `--fraction`](#ตัวอย่าง-flags)
7. [Special Cases: no solution / infinite solutions](#special-cases)

---

## 1. ภาพรวม

`simplify` จัด**สมการให้อยู่ในรูป canonical** เช่น `Ax + By = C` มันไม่ "แก้" สมการ (ต่างจาก `solve` ที่หาค่าตัวแปร)

```
User Input: "simplify 3x + 2 + x = 10 - x"
  → dispatch() ตรวจ starts_with("simplify ") → true
  → cmd_simplify("3x + 2 + x = 10 - x", g_config, g_ctx)
```

**Output ที่คาดหวัง:** `  5x = 8`

---

## Step 1 — Parse Flags

```cpp
// command.hpp — cmd_simplify()
CommandFlags flags = parse_flags(args);
// args = "3x + 2 + x = 10 - x"
```

`parse_flags()` แบ่ง input เป็น 2 ส่วน: **expression** กับ **flags**

```cpp
// command.hpp — parse_flags()
CommandFlags parse_flags(const string& input) {
    CommandFlags flags;
    size_t flag_start = input.find(" --");   // หา " --" แรก
    if (flag_start == string::npos) {
        flags.expression = input;             // ไม่มี flags → expression = ทั้งหมด
        return flags;
    }
    flags.expression = trim(input.substr(0, flag_start));  // ส่วนก่อน --
    // parse --vars, --isolated, --fraction จาก flag_part
}
```

**ตัวอย่าง:**

| Input | `flags.expression` | `flags.vars` | `flags.isolated` | `flags.fraction` |
|-------|-------------------|-------------|----------------|-|
| `3x + 2 = 10` | `3x + 2 = 10` | `[]` | `false` | `false` |
| `3x + 2y = 10 --vars y x` | `3x + 2y = 10` | `["y", "x"]` | `false` | `false` |
| `x + 1 = 4 --isolated --fraction` | `x + 1 = 4` | `[]` | `true` | `true` |

**ตรวจ fraction_mode จาก config:**

```cpp
// ถ้าผู้ใช้ไม่ได้ระบุ --fraction แต่ config มี fraction_mode = true → เปิดให้อัตโนมัติ
if (!flags.fraction && g_config.settings().fraction_mode)
    flags.fraction = true;
```

---

## Step 2 — Shadow Detection

**ปัญหา:** ถ้า `g_ctx = {x: 5}` แล้วสั่ง `simplify 2x + 3 = 7` → `x` จะถูก substitute เป็น `5` → ได้ `13 = 7` ซึ่งอาจไม่ใช่สิ่งที่ผู้ใช้ต้องการ

**Simplifier** จึงตรวจและ **แจ้งเตือน** ก่อน:

```cpp
// simplify.hpp — simplify()
if (context_ && !opts.isolated) {
    // First pass: collect ตัวแปรทั้งหมด (ไม่แทนค่า)
    LinearCollector shadow_check(nullptr, input_, true);
    //                           ^^^^^^^         ^^^^
    //                           context=null    isolated=true

    LinearForm lhs_vars = shadow_check.collect(eq.lhs());
    LinearForm rhs_vars = shadow_check.collect(eq.rhs());
    LinearForm all_vars = lhs_vars - rhs_vars;
    // all_vars.variables() = {"x"}  (ตัวแปรทั้งหมดที่มีในสมการ)

    for (const auto& var : all_vars.variables()) {
        if (context_->has(var)) {
            result.warnings.insert(
                "'x' in expression shadows context variable "
                "(use --isolated to keep as variable)");
        }
    }
}
```

**Output warning:** `  Warning: 'x' in expression shadows context variable (use --isolated to keep as variable)`

---

## Step 3 — Collect & Normalize

### Input: `3x + 2 + x = 10 - x` (ไม่มี context substitution)

```cpp
// simplify.hpp
LinearCollector collector(opts.isolated ? nullptr : context_, input_, false);

LinearForm lhs = collector.collect(eq.lhs());   // "3x + 2 + x"
LinearForm rhs = collector.collect(eq.rhs());   // "10 - x"
LinearForm normalized = lhs - rhs;
normalized.simplify();
```

**collect LHS: `3x + 2 + x`**

AST:
```
BinaryOp(Add)
├── BinaryOp(Add)
│   ├── BinaryOp(Mul, Number(3), Variable(x))
│   └── Number(2)
└── Variable(x)
```

```
visit BinaryOp(Add)                   ← root
 ├── visit BinaryOp(Add)              ← "3x + 2"
 │    ├── visit BinaryOp(Mul)         ← "3x"
 │    │    ├── visit Number(3)  → {coeffs:{}, constant:3}
 │    │    ├── visit Variable(x) → {coeffs:{"x":1}, constant:0}
 │    │    └── Mul: left.is_constant() → result_ = right * 3
 │    │         = {coeffs:{"x":3}, constant:0}
 │    │
 │    ├── left = {coeffs:{"x":3}, constant:0}
 │    ├── visit Number(2) → {coeffs:{}, constant:2}
 │    ├── right = {coeffs:{}, constant:2}
 │    └── Add: {coeffs:{"x":3}, constant:2}
 │
 ├── left = {coeffs:{"x":3}, constant:2}
 │
 ├── visit Variable(x) → {coeffs:{"x":1}, constant:0}
 ├── right = {coeffs:{"x":1}, constant:0}
 │
 └── Add: coeffs["x"] = 3+1 = 4, constant = 2+0 = 2
     result_ = {coeffs:{"x":4}, constant:2}
```

**lhs = `{coeffs:{"x":4}, constant:2}`** → แปลว่า `4x + 2`

**collect RHS: `10 - x`**

```
visit BinaryOp(Sub)
 ├── visit Number(10) → {coeffs:{}, constant:10}
 ├── visit Variable(x) → {coeffs:{"x":1}, constant:0}
 └── Sub: coeffs["x"] = 0-1 = -1, constant = 10-0 = 10
     result_ = {coeffs:{"x":-1}, constant:10}
```

**rhs = `{coeffs:{"x":-1}, constant:10}`** → แปลว่า `-x + 10`

**Normalize: lhs - rhs**

```
normalized = {coeffs:{"x":4}, constant:2} - {coeffs:{"x":-1}, constant:10}

coeffs["x"] = 4 - (-1) = 5
constant    = 2 - 10    = -8

normalized = {coeffs:{"x":5}, constant:-8}
```

**ความหมาย:** `5x + (-8) = 0` → `5x = 8`

---

## Step 4 — Format Canonical

```cpp
result.canonical = format_canonical(normalized, result.var_order, opts);
```

`format_canonical()` ทำ:
1. **ฝั่งซ้าย:** วาง term ตัวแปร → `5x`
2. **ฝั่งขวา:** negate constant → `rhs = -(-8) = 8`
3. **ประกอบ:** `"5x = 8"`

```cpp
// format_canonical()
// normalized = {coeffs:{"x":5}, constant:-8}

// Left side: loop ตาม var_order
for (const auto& var : var_order) {       // var_order = ["x"]
    double coeff = form.get_coeff(var);   // coeff = 5
    // first = true, coeff >= 0 → ไม่ใส่ sign
    // coeff = 5, ไม่ใช่ 1 → coeff_str = "5"
    oss << "5" << "x";                    // "5x"
    first = false;
}

// Right side
double rhs = -form.constant;             // rhs = -(-8) = 8
oss << " = " << "8";                     // " = 8"

// result = "5x = 8"
```

**Output:**
```
  5x = 8
```

---

## ตัวอย่าง Flags

### `--vars y x` (กำหนดลำดับตัวแปร)

```
Input: simplify 3x + 2y = 10 --vars y x
```

- `flags.vars = ["y", "x"]`
- `opts.var_order = ["y", "x"]`
- normalized = `{coeffs:{"x":3, "y":2}, constant:-10}`
- format ตาม `var_order`:
  - y → coeff 2 → `"2y"`
  - x → coeff 3 → `" + 3x"`
- **Output:** `  2y + 3x = 10`

ถ้า**ไม่ระบุ** `--vars` → auto sort alphabetically → `"3x + 2y = 10"`

### `--isolated` (ไม่แทนค่าจาก context)

```
Input: simplify 2x + y = 10 --isolated
สมมติ g_ctx = {y: 3}
```

- `opts.isolated = true`
- `collector` ใช้ `context = nullptr` → **ไม่** substitute `y = 3`
- normalized = `{coeffs:{"x":2, "y":1}, constant:-10}`
- **Output:** `  2x + y = 10`

ถ้า**ไม่ระบุ** `--isolated`:
- `y` ถูก substitute → `y = 3` → `2x + 3 = 10`
- normalized = `{coeffs:{"x":2}, constant:-7}`
- **Output:** `  2x = 7`

### `--fraction` (แสดงเป็นเศษส่วน)

```
Input: simplify 3x = 7 --fraction
```

- normalized = `{coeffs:{"x":3}, constant:-7}`
- rhs = 7/3 → format เป็น fraction: `Fraction(7, 3)` → `"7/3"`
- **Output:** `  x = 7/3`

(เทียบกับไม่มี `--fraction`: `  x = 2.333333`)

---

## Special Cases

### No Solution

```
Input: simplify 5 = 3
```

- lhs = `{constant:5}`, rhs = `{constant:3}`
- normalized = `{constant:2}` → `is_constant() = true`, `|2| > 1e-12`
- `result.is_no_solution() → true`

```cpp
if (result.is_no_solution()) {
    cout << "  " << result.canonical << "\n";       // "  0 = -2"
    cout << "  => no solution\n";                    // สีแดง
}
```

### Infinite Solutions

```
Input: simplify x + 1 = x + 1
```

- lhs = `{coeffs:{"x":1}, constant:1}`, rhs = เหมือนกัน
- normalized = `{coeffs:{"x":0}, constant:0}` → simplify → `{constant:0}`
- `is_constant() = true`, `|0| < 1e-12`
- `result.is_infinite_solutions() → true`

```cpp
if (result.is_infinite_solutions()) {
    cout << "  " << result.canonical << "\n";       // "  0 = 0"
    cout << "  => infinite solutions\n";             // สีเขียว
}
```

### Non-linear equation

```
Input: simplify x^2 + 3 = 12
```

- `LinearCollector` เจอ `x^2` → exponent = 2 → throw `NonLinearError`
- Output: error message
