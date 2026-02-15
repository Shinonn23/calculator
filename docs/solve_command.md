# `solve` Command — Walkthrough แบบละเอียด

## สารบัญ
1. [ภาพรวม (Overview)](#1-ภาพรวม)
2. [Step 1 — Parse สมการเป็น AST](#step-1--parse-สมการเป็น-ast)
3. [Step 2 — Collect เป็น LinearForm (ไม่แทนค่าตัวแปร)](#step-2--collect-เป็น-linearform)
4. [Step 3 — รวม LHS - RHS เป็นฟอร์มเดียว](#step-3--รวม-lhs---rhs-เป็นฟอร์มเดียว)
5. [Step 4 — จัดการ Context & Shadowing](#step-4--จัดการ-context--shadowing)
6. [Step 5 — EquationSolver แก้สมการ](#step-5--equationsolver-แก้สมการ)
7. [Step 6 — บันทึกผลลัพธ์ & แสดงผล](#step-6--บันทึกผลลัพธ์--แสดงผล)

---

## 1. ภาพรวม

เมื่อผู้ใช้พิมพ์ `solve 2x + 3 = 7` โปรแกรมจะเรียก `cmd_solve("2x + 3 = 7", g_ctx, g_config)` ซึ่งอยู่ใน `command.hpp` ฟังก์ชันนี้ทำงานเป็น 6 ขั้นตอนดังนี้:

```
User Input: "2x + 3 = 7"
  │
  ├─ Step 1: Parser     ──→  AST (Equation node)
  ├─ Step 2: Collector  ──→  LinearForm (หา coefficients ของตัวแปร)
  ├─ Step 3: Combine    ──→  LHS - RHS = single LinearForm
  ├─ Step 4: Shadowing  ──→  สร้าง temp_ctx เพื่อไม่ให้ตัวแปรเป้าหมายถูกแทนค่า
  ├─ Step 5: Solver     ──→  แก้สมการ ax + b = 0 → x = -b/a
  └─ Step 6: Output     ──→  บันทึกค่าลง g_ctx + แสดงผล
```

> [!NOTE]
> สมมติว่า `g_ctx` (Context หลัก) มีค่า `{ y: 3 }` อยู่แล้วก่อนรัน solve

---

## Step 1 — Parse สมการเป็น AST

```cpp
// command.hpp — cmd_solve()
Parser  parser(args);                // args = "2x + 3 = 7"
auto    eq = parser.parse_equation();
```

### เกิดอะไรขึ้นข้างใน `Parser`?

**1.1** Constructor `Parser("2x + 3 = 7")` สร้าง `Lexer` ซึ่งจะ **tokenize** string ออกมาเป็น token list:

```
Token stream:
  Number(2), Variable(x), Plus, Number(3), Equals, Number(7), End
```

> `2x` ถูก Lexer แยกเป็น 2 tokens: `Number(2)` กับ `Variable(x)` แล้ว Parser จะรู้ว่ามี **implicit multiplication** (2 * x)

**1.2** เรียก `parse_equation()` ซึ่งทำงานดังนี้:

```cpp
// parser.hpp — parse_equation()
EquationPtr parse_equation() {
    auto lhs = parse_expression();   // ◀── parse ฝั่งซ้าย "2x + 3"
    // ....expect Equals token....
    advance();                        // ◀── กินเครื่องหมาย "=" ออก
    auto rhs = parse_expression();   // ◀── parse ฝั่งขวา "7"
    return make_unique<Equation>(move(lhs), move(rhs));
}
```

**1.3** `parse_expression()` ภายในใช้ **precedence climbing** แบ่งเป็นชั้นๆ:

```
parse_expression() → parse_additive()
  → parse_multiplicative()  → parse_power() → parse_unary() → parse_primary()
```

สำหรับ `"2x + 3"`:
- `parse_primary()` อ่าน `Number(2)` → return `Number(2)` node
- แล้วพบ token `Variable(x)` ต่อกัน → เป็น implicit multiply → สร้าง `BinaryOp(Number(2), Mul, Variable(x))`
- กลับขึ้นมาที่ `parse_additive()` → พบ `Plus` → อ่าน `Number(3)`
- สร้าง `BinaryOp(..., Add, Number(3))`

**ผลลัพธ์ AST ของ `eq`:**

```
Equation
├── lhs: BinaryOp(Add)
│       ├── left:  BinaryOp(Mul)
│       │           ├── left:  Number(2)
│       │           └── right: Variable("x")
│       └── right: Number(3)
└── rhs: Number(7)
```

> [!TIP]
> สังเกตว่า `eq->lhs()` จะได้ `BinaryOp(Add)` ทั้ง subtree ส่วน `eq->rhs()` จะได้ `Number(7)`

---

## Step 2 — Collect เป็น LinearForm

```cpp
// command.hpp — cmd_solve()
LinearCollector var_finder(nullptr, args, true);
//                         ^^^^^^         ^^^^
//                         context=null   isolated=true (ไม่แทนค่าตัวแปรจาก context)
//
// ⚠️ ตรงนี้ส่ง nullptr เข้าไป → ห้าม substitute ค่าจาก context เลย
//    ทำแบบนี้เพราะเราต้องการ "สำรวจ" ว่ามีตัวแปรอะไรบ้างในสมการก่อน

LinearForm  lhs_form = var_finder.collect(eq->lhs());
LinearForm  rhs_form = var_finder.collect(eq->rhs());
```

### 2.1 — collect ฝั่ง LHS: `2x + 3`

เรียก `var_finder.collect(eq->lhs())`:

```cpp
// linear_collector.hpp — collect()
LinearForm collect(const Expr& expr) {
    result_ = LinearForm();          // reset: { coeffs: {}, constant: 0 }
    shadowed_vars_.clear();          // clear set
    expr.accept(*this);              // ◀── เริ่ม Visitor Pattern
    result_.simplify();              // ลบ coefficient ที่เป็น ~0 ออก
    return result_;
}
```

**Visitor Pattern** — เมื่อเรียก `expr.accept(*this)` ตัว AST node จะ dispatch กลับมาเรียก `visit()` ที่เหมาะสมกับ type ของมัน:

- `Number` node → เรียก `visitor.visit(const Number&)`
- `Variable` node → เรียก `visitor.visit(const Variable&)`
- `BinaryOp` node → เรียก `visitor.visit(const BinaryOp&)`

เนื่องจาก `lhs` คือ `BinaryOp(Add)` จึง dispatch ไปที่:

```cpp
void visit(const BinaryOp& node) override {
    node.left().accept(*this);      // ◀── ลงไป visit ลูกซ้าย
    LinearForm left = result_;      //     เก็บผลลัพธ์จากลูกซ้าย

    node.right().accept(*this);     // ◀── ลงไป visit ลูกขวา
    LinearForm right = result_;     //     เก็บผลลัพธ์จากลูกขวา

    switch (node.op()) {
    case Add: result_ = left + right; break;
    case Sub: result_ = left - right; break;
    case Mul: /* ... */
    // ...
    }
}
```

**ลำดับการ visit ทั้งหมด (depth-first):**

```
visit BinaryOp(Add)                 ← root ของ LHS
 ├── visit BinaryOp(Mul)            ← ลูกซ้าย "2x"
 │    ├── visit Number(2)
 │    │     → result_ = LinearForm(2.0)
 │    │       ตอนนี้ result_ = { coeffs: {}, constant: 2 }
 │    │
 │    ├── left = { coeffs: {}, constant: 2 }
 │    │
 │    ├── visit Variable("x")
 │    │     → context_ == nullptr ดังนั้นไม่แทนค่า
 │    │     → result_ = LinearForm("x", 1.0)
 │    │       ตอนนี้ result_ = { coeffs: {"x": 1}, constant: 0 }
 │    │
 │    ├── right = { coeffs: {"x": 1}, constant: 0 }
 │    │
 │    └── op = Mul
 │        ✓ left.is_constant() == true (ไม่มี coeffs)
 │        → result_ = right * left.constant
 │                   = { coeffs: {"x": 1}, constant: 0 } * 2
 │                   = { coeffs: {"x": 2}, constant: 0 }
 │
 ├── left = { coeffs: {"x": 2}, constant: 0 }
 │
 ├── visit Number(3)
 │     → result_ = LinearForm(3.0)
 │       ตอนนี้ result_ = { coeffs: {}, constant: 3 }
 │
 ├── right = { coeffs: {}, constant: 3 }
 │
 └── op = Add
     → result_ = left + right
                = { coeffs: {"x": 2}, constant: 0 }
                + { coeffs: {},       constant: 3 }
                = { coeffs: {"x": 2}, constant: 3 }
```

**สรุป: `lhs_form = { coeffs: {"x": 2}, constant: 3 }`** — แปลว่า `2x + 3`

### 2.2 — collect ฝั่ง RHS: `7`

```
visit Number(7)
  → result_ = LinearForm(7.0)
  → result_ = { coeffs: {}, constant: 7 }
```

**สรุป: `rhs_form = { coeffs: {}, constant: 7 }`** — แปลว่า `7` (ค่าคงที่ ไม่มีตัวแปร)

---

## Step 3 — รวม LHS - RHS เป็นฟอร์มเดียว

```cpp
LinearForm combined = lhs_form - rhs_form;
```

**คำนวณทีละส่วน:**

```cpp
// LinearForm::operator-()
LinearForm operator-(const LinearForm& other) const {
    LinearForm result = *this;             // copy lhs_form
    result.constant -= other.constant;     // 3 - 7 = -4
    for (const auto& pair : other.coeffs)  // rhs ไม่มี coeffs → ไม่ทำอะไร
        result.coeffs[pair.first] -= pair.second;
    return result;
}
```

**ผลลัพธ์:**
```
combined = { coeffs: {"x": 2}, constant: -4 }
```
ความหมาย: `2x + (-4) = 0` → `2x - 4 = 0`

---

**หา variables ที่มีในสมการ:**

```cpp
auto eq_vars = combined.variables();
// variables() วนดู coeffs ทุกตัว เก็บเฉพาะที่ |coefficient| > 1e-12
// coeffs: {"x": 2}  →  |2| > 1e-12 → ใส่ "x"
// ผลลัพธ์: eq_vars = {"x"}
```

---

## Step 4 — จัดการ Context & Shadowing

**ปัญหา:** ถ้า `g_ctx` มี `x = 5` อยู่แล้ว แล้วผู้ใช้สั่ง `solve 2x + 3 = 7` — ตัว solver จะ substitute `x = 5` เข้าไปแทน ทำให้แก้สมการไม่ได้ (เพราะไม่เหลือ unknown variable)

**วิธีแก้:** ถ้าสมการมีตัวแปรเดียว (`eq_vars.size() == 1`) และตัวแปรนั้นถูก define ไว้ใน `g_ctx` → สร้าง `temp_ctx` โดย copy ทุกตัวแปรยกเว้นตัวเป้าหมาย

```cpp
Context         temp_ctx;
const Context*  solve_ctx = &g_ctx;       // ตั้งต้น: ชี้ไปที่ g_ctx หลัก
if (eq_vars.size() == 1) {                // eq_vars: {"x"} → size = 1 ✓
    string target = *eq_vars.begin();     // target = "x"
    if (g_ctx.has(target)) {              // สมมติ g_ctx = {x:5, y:3}
                                          // g_ctx.has("x") = true ✓
        for (const auto& [name, value] : g_ctx.all()) {
            if (name != target)           // copy ทุกตัวยกเว้น "x"
                temp_ctx.set(name, value);
        }
        solve_ctx = &temp_ctx;            // switch ไปใช้ temp_ctx
    }
}
```

**ตัวอย่างผลลัพธ์:**

| สถานะ | g_ctx                  | temp_ctx | solve_ctx ชี้ไปที่ |
| ----- | ---------------------- | -------- | -------------- |
| ก่อน   | `{x:5, y:3}`           | `{}`     | `&g_ctx`       |
| หลัง   | `{x:5, y:3}` (ไม่เปลี่ยน) | `{y:3}`  | `&temp_ctx`    |

> [!IMPORTANT]
> **ทำไมต้องเก็บ `y` ไว้?** — เพราะถ้าสมการคือ `solve 2x + y = 10` แล้ว `y` ถูก define ไว้ใน context → solver ต้องแทน `y = 3` เข้าไป เหลือแค่ `x` ตัวเดียวเป็น unknown จึง solve ได้

> **ทำไมต้อง exclude `x`?** — เพราะ `x` คือตัวที่เราต้องการหาค่า ถ้าแทนค่าเข้าไปจะไม่เหลือ unknown เลย

**กรณีที่ g_ctx ไม่มี `x`:** (เช่น `g_ctx = {y: 3}`)
- `g_ctx.has("x")` = false → ไม่สร้าง `temp_ctx` → `solve_ctx` ยังชี้ไปที่ `&g_ctx` ตามเดิม

---

## Step 5 — EquationSolver แก้สมการ

```cpp
EquationSolver solver(solve_ctx, args);
//                     ^^^^^^^^  ^^^^
//                     pointer ไปยัง context ที่เลือก (temp_ctx หรือ g_ctx)
//                     args = "2x + 3 = 7" (เก็บไว้สำหรับ error message)

SolveResult result = solver.solve(*eq);
```

### 5.1 — เข้าไปใน `EquationSolver::solve()`

```cpp
// solver.hpp — solve()
SolveResult solve(const Equation& eq) {
    // สร้าง LinearCollector ตัวใหม่ — คราวนี้ใช้ context จริง (ไม่ isolated)
    LinearCollector collector(context_, input_, false);
    //                        ^^^^^^^^          ^^^^^
    //                        = solve_ctx       isolated = false
    //                        (ถ้าเป็น temp_ctx จะมีแค่ {y:3} ไม่มี x)

    LinearForm lhs = collector.collect(eq.lhs());
    LinearForm rhs = collector.collect(eq.rhs());
    // ...
}
```

> [!NOTE]
> สังเกตว่า EquationSolver สร้าง `LinearCollector` ตัวใหม่ด้วย `isolated = false` ซึ่งจะ **substitute** ตัวแปรจาก context ลงไป — ต่างจาก Step 2 ที่ใช้ `isolated = true` เพื่อแค่นับตัวแปร

### 5.2 — collect อีกรอบ (คราวนี้แทนค่า)

สำหรับ input `2x + 3 = 7` ที่ `solve_ctx` ไม่มี `x`:

**collect LHS (`2x + 3`):**

```
visit BinaryOp(Add)
 ├── visit BinaryOp(Mul)
 │    ├── visit Number(2)   → result_ = { coeffs:{}, constant:2 }
 │    ├── visit Variable(x) → context_->has("x")? → No (ไม่มีใน solve_ctx)
 │    │                     → result_ = { coeffs:{"x":1}, constant:0 }
 │    └── Mul: left is constant → result_ = { coeffs:{"x":2}, constant:0 }
 │
 ├── visit Number(3) → result_ = { coeffs:{}, constant:3 }
 └── Add: result_ = { coeffs:{"x":2}, constant:3 }
```

**collect RHS (`7`):**
```
visit Number(7) → result_ = { coeffs:{}, constant:7 }
```

### 5.3 — Normalize เป็น `= 0`

```cpp
LinearForm normalized = lhs - rhs;
// { coeffs:{"x":2}, constant:3 } - { coeffs:{}, constant:7 }
// = { coeffs:{"x":2}, constant: 3-7 }
// = { coeffs:{"x":2}, constant: -4 }

normalized.simplify();
// |2| > 1e-12 → keep "x"
// |-4| > 1e-12 → keep constant
// ไม่มีอะไรเปลี่ยน
```

**ความหมาย:** `2x + (-4) = 0` → `2x = 4`

### 5.4 — หา unknowns & แก้สมการ

```cpp
set<string> unknowns = normalized.variables();
// coeffs: {"x":2} → unknowns = {"x"}
// unknowns.size() = 1 → ผ่าน ✓ (ถ้า > 1 จะ throw MultipleUnknownsError)

// Single unknown: ax + b = 0 → x = -b/a
string var = *unknowns.begin();         // var = "x"
double a   = normalized.get_coeff(var); // a = 2    (coefficient ของ x)
double b   = normalized.constant;       // b = -4   (ค่า constant)

// ตรวจว่า a ≠ 0
// |2| > 1e-12 → ผ่าน ✓

SolveResult result;
result.variable     = var;              // "x"
result.value        = -b / a;           // -(-4) / 2 = 4 / 2 = 2
result.has_solution = true;
return result;
```

**ผลลัพธ์: `SolveResult { variable: "x", value: 2.0, has_solution: true }`**

> [!TIP]
> **สูตรหลัก:** จาก `ax + b = 0` จะได้ `x = -b / a`
> - `a` = coefficient ของตัวแปร (ในที่นี้ = 2)
> - `b` = ค่า constant (ในที่นี้ = -4)
> - `x = -(-4) / 2 = 2` ✓

---

## Step 6 — บันทึกผลลัพธ์ & แสดงผล

```cpp
// command.hpp — cmd_solve() (กลับมาจาก solver.solve())
if (result.has_solution) {                              // true ✓
    g_ctx.set(result.variable, result.value);           // g_ctx.set("x", 2.0)
    // ⚡ ตอนนี้ g_ctx = {x: 2, y: 3}
    //    ค่าเดิม x=5 ถูก overwrite เป็น x=2

    cout << "  " << result.variable << " = "
         << fmt(result.value, g_config)                 // format ตัวเลข (เช่น decimal places)
         << ansi::dim << "  (saved)"
         << ansi::reset << "\n";
    // Output: "  x = 2  (saved)"
} else {
    cout << "  " << result.to_string() << "\n";         // "no solution"
}
```

**ผลลัพธ์ที่ผู้ใช้เห็น:**
```
  x = 2  (saved)
```

---

## ตัวอย่างเพิ่มเติม: สมการที่มีหลายตัวแปร + context

### Input: `solve 3x + 2y = 12` where `g_ctx = {y: 3}`

| Step                   | ค่าสำคัญ                                                                                 |
| ---------------------- | ------------------------------------------------------------------------------------- |
| **Parse**              | `Equation(BinaryOp(Add, BinaryOp(Mul, 3, x), BinaryOp(Mul, 2, y)), Number(12))`       |
| **Collect (isolated)** | `lhs_form = {coeffs: {x:3, y:2}, constant:0}`, `rhs_form = {coeffs: {}, constant:12}` |
| **Combine**            | `combined = {coeffs: {x:3, y:2}, constant:-12}`                                       |
| **eq_vars**            | `{"x", "y"}` — มี 2 ตัว → `eq_vars.size() != 1`                                         |
| **Shadowing**          | **ข้าม** — ไม่สร้าง temp_ctx → `solve_ctx = &g_ctx = {y:3}`                              |
| **Solver collect**     | ใน `EquationSolver::solve()` ใช้ `isolated=false` + `context = {y:3}`                  |
|                        | visit Variable("y") → `context->has("y")` = true → substitute → `LinearForm(3.0)`     |
|                        | visit Variable("x") → `context->has("x")` = false → keep → `LinearForm("x", 1.0)`     |
|                        | `lhs = {coeffs: {x:3}, constant:6}` (เพราะ `2y` ถูกแทนเป็น `2*3=6`)                     |
|                        | `rhs = {coeffs: {}, constant:12}`                                                     |
| **Normalize**          | `normalized = {coeffs: {x:3}, constant: 6-12} = {coeffs: {x:3}, constant:-6}`         |
| **Solve**              | `a=3, b=-6` → `x = -(-6)/3 = 2`                                                       |
| **Output**             | `x = 2  (saved)`                                                                      |

---

## Error Cases

### กรณี unknowns > 1 (ตัวแปรหลายตัวที่ไม่ได้ define)

```
solve 3x + 2y = 12   (g_ctx ว่าง → ไม่มีค่า y)
```
- `eq_vars = {"x", "y"}`, size = 2 → ข้าม shadowing
- Solver: `isolated=false` แต่ context ไม่มี y → เหลือ unknowns = `{"x", "y"}`
- **Throw `MultipleUnknownsError`** → ขึ้น error + Hint: `use :set to define variables`

### กรณี constant equation (ไม่มี unknown)

```
solve 5 = 5   →   normalized = { coeffs:{}, constant:0 }
```
- unknowns is empty → check: `|0| < 1e-12` → true → **InfiniteSolutionsError** ("equation is always true")

```
solve 5 = 3   →   normalized = { coeffs:{}, constant:2 }
```
- unknowns is empty → check: `|2| < 1e-12` → false → **NoSolutionError** ("2 != 0")

### กรณี non-linear (ตัวแปรกำลังสอง)

```
solve x^2 + 3 = 12
```
- LinearCollector เจอ `BinaryOp(Pow)` → exponent = 2 → left ไม่ใช่ constant
- **Throw `NonLinearError`** ("non-linear term: variable raised to power 2")
