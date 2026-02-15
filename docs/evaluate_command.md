# Evaluate (คำนวณ Expression / ตรวจสอบ Equation) — Walkthrough แบบละเอียด

## สารบัญ
1. [ภาพรวม (Overview)](#1-ภาพรวม)
2. [Flow: พิมพ์ expression → ได้ผลลัพธ์](#flow-a--expression-ธรรมดา)
3. [Flow: พิมพ์ equation → ตรวจสอบ true/false](#flow-b--equation-ตรวจสอบ)
4. [Evaluator — Visitor Pattern ทีละ node](#evaluator--visitor-pattern)
5. [ตัวอย่างที่มีตัวแปรจาก Context](#ตัวอย่างที่มีตัวแปรจาก-context)
6. [Error Cases](#error-cases)

---

## 1. ภาพรวม

เมื่อผู้ใช้พิมพ์อะไรก็ตามที่ **ไม่ใช่ command** (ไม่ขึ้นต้นด้วย `:`, `solve`, `simplify`, `exit`) REPL จะเรียก `cmd_evaluate()` โดยอัตโนมัติ

```
User Input: "2 + 3 * 4"    →  cmd_evaluate("2 + 3 * 4", g_ctx, g_config)
User Input: "10 = 5 + 5"   →  cmd_evaluate("10 = 5 + 5", g_ctx, g_config)
```

```cpp
// main.cpp — dispatch()
// ถ้าไม่ match กับ command ใดๆ → ตกลงมาที่นี่:
cmd_evaluate(input, g_ctx, g_config);
```

---

## Flow A — Expression ธรรมดา

**Input:** `2 + 3 * 4`

### Step 1 — Parse ด้วย `parse_expression_or_equation()`

```cpp
// command.hpp — cmd_evaluate()
Parser parser(input);                                  // input = "2 + 3 * 4"
auto [expr, eq] = parser.parse_expression_or_equation();
```

`parse_expression_or_equation()` ต่างจาก `parse_equation()` ตรงที่มันจะ **ลองอ่านก่อน** ว่ามีเครื่องหมาย `=` ไหม:

```cpp
// parser.hpp
pair<ExprPtr, EquationPtr> parse_expression_or_equation() {
    auto lhs = parse_expression();       // parse "2 + 3 * 4"
    if (current_.type == TokenType::Equals) {
        // ... มี = → parse ฝั่งขวา → return {nullptr, Equation}
    }
    // ไม่มี = → return {ExprPtr, nullptr}
    return {move(lhs), nullptr};
}
```

**ผลลัพธ์:** `expr` ≠ nullptr, `eq` = nullptr → เป็น **expression** ธรรมดา

**AST ที่ได้:**

```
BinaryOp(Add)
├── left:  Number(2)
└── right: BinaryOp(Mul)
           ├── left:  Number(3)
           └── right: Number(4)
```

> สังเกตว่า `*` มี precedence สูงกว่า `+` → `3 * 4` ถูก parse ก่อนแล้วเป็นลูกขวาของ `+`

### Step 2 — Evaluate ด้วย Evaluator

```cpp
// command.hpp — cmd_evaluate()
if (eq) { /* ... */ }
else {
    Evaluator eval(&g_ctx, input);           // สร้าง Evaluator
    double    result = eval.evaluate(*expr); // ◀── คำนวณ
    cout << "  = " << fmt(result, g_config) << "\n";
}
```

```cpp
// evaluator.hpp
double evaluate(const Expr& expr) {
    expr.accept(*this);   // ◀── Visitor Pattern เหมือน LinearCollector
    return result_;
}
```

**ลำดับการ visit:**

```
visit BinaryOp(Add)
 ├── visit Number(2)     → result_ = 2
 ├── left_val = 2
 ├── visit BinaryOp(Mul)
 │    ├── visit Number(3)  → result_ = 3
 │    ├── left_val = 3
 │    ├── visit Number(4)  → result_ = 4
 │    ├── right_val = 4
 │    └── Mul: result_ = 3 * 4 = 12
 ├── right_val = 12
 └── Add: result_ = 2 + 12 = 14
```

**Output:**
```
  = 14
```

---

## Flow B — Equation (ตรวจสอบ)

**Input:** `10 = 5 + 5`

### Step 1 — Parse เป็น Equation

```cpp
auto [expr, eq] = parser.parse_expression_or_equation();
// parser เห็น = → parse ทั้ง lhs และ rhs
// expr = nullptr, eq ≠ nullptr
```

**AST ที่ได้:**
```
Equation
├── lhs: Number(10)
└── rhs: BinaryOp(Add)
         ├── left:  Number(5)
         └── right: Number(5)
```

### Step 2 — Evaluate ทั้งสองฝั่ง แล้วเปรียบเทียบ

```cpp
if (eq) {
    Evaluator eval(&g_ctx, input);
    double lhs_val = eval.evaluate(eq->lhs());   // evaluate Number(10) → 10
    double rhs_val = eval.evaluate(eq->rhs());   // evaluate 5+5       → 10

    cout << "  " << fmt(lhs_val, g_config) << " = " << fmt(rhs_val, g_config);
    //      "  10 = 10"

    if (abs(lhs_val - rhs_val) < 1e-12) {
        // |10 - 10| = 0 < 1e-12 → จริง!
        cout << "  (true)\n";      // สีเขียว
    } else {
        cout << "  (false)\n";     // สีแดง
    }
}
```

**Output:**
```
  10 = 10  (true)
```

---

## Evaluator — Visitor Pattern

`Evaluator` ใช้ Visitor Pattern **เหมือนกับ `LinearCollector`** แต่ง่ายกว่ามาก — เพราะมันแค่ **คำนวณตัวเลข** ไม่ต้อง track coefficients

### visit(Number)
```cpp
void Evaluator::visit(const Number& node) {
    result_ = node.value();    // เก็บค่าตัวเลขตรงๆ
}
```

### visit(Variable)
```cpp
void Evaluator::visit(const Variable& node) {
    if (!context_ || !context_->has(node.name())) {
        throw UndefinedVariableError(node.name(), ...);
        //     ⚠️ ต่างจาก LinearCollector ที่เก็บเป็น coefficient
        //     Evaluator ต้องการค่าจริง ถ้าไม่มี → error ทันที
    }
    result_ = context_->get(node.name());   // ดึงค่าจาก context
}
```

### visit(BinaryOp)
```cpp
void Evaluator::visit(const BinaryOp& node) {
    node.left().accept(*this);
    double left_val = result_;        // เก็บผลจากฝั่งซ้าย

    node.right().accept(*this);
    double right_val = result_;       // เก็บผลจากฝั่งขวา

    switch (node.op()) {
    case Add: result_ = left_val + right_val;  break;
    case Sub: result_ = left_val - right_val;  break;
    case Mul: result_ = left_val * right_val;  break;
    case Div:
        if (right_val == 0)
            throw MathError("division by zero", ...);
        result_ = left_val / right_val;
        break;
    case Pow: result_ = pow(left_val, right_val); break;
    }
}
```

> **ข้อแตกต่างหลักจาก `LinearCollector`:**
> | | Evaluator | LinearCollector |
> |---|---|---|
> | `result_` type | `double` | `LinearForm` |
> | Variable ไม่มีค่า | throw error | เก็บเป็น coefficient |
> | `x * y` | ได้เลย (ถ้ามีค่า) | throw NonLinearError |

---

## ตัวอย่างที่มีตัวแปรจาก Context

**สมมติ:** `g_ctx = {x: 5, y: 3}`

### Input: `x + y * 2`

```
visit BinaryOp(Add)
 ├── visit Variable("x")
 │    → context_->has("x") = true
 │    → result_ = context_->get("x") = 5
 ├── left_val = 5
 ├── visit BinaryOp(Mul)
 │    ├── visit Variable("y")
 │    │    → context_->has("y") = true
 │    │    → result_ = context_->get("y") = 3
 │    ├── left_val = 3
 │    ├── visit Number(2) → result_ = 2
 │    ├── right_val = 2
 │    └── Mul: result_ = 3 * 2 = 6
 ├── right_val = 6
 └── Add: result_ = 5 + 6 = 11
```

**Output:** `  = 11`

### Input: `x^2 + y^2 = 34` (ตรวจสอบ equation)

```
lhs: x^2 + y^2
  visit BinaryOp(Add)
   ├── visit BinaryOp(Pow)
   │    ├── visit Variable("x") → 5
   │    ├── visit Number(2)     → 2
   │    └── Pow: pow(5, 2) = 25
   ├── visit BinaryOp(Pow)
   │    ├── visit Variable("y") → 3
   │    ├── visit Number(2)     → 2
   │    └── Pow: pow(3, 2) = 9
   └── Add: 25 + 9 = 34

rhs: Number(34) → 34

|34 - 34| = 0 < 1e-12 → true ✓
```

**Output:** `  34 = 34  (true)`

---

## Error Cases

### ตัวแปรที่ไม่ได้ define

```
Input: "a + 5"    (g_ctx ไม่มี a)
```
- visit Variable("a") → `context_->has("a")` = false
- **throw `UndefinedVariableError("a")`**
- Output: error message พร้อม caret `^` ชี้ตำแหน่ง `a`

### หารด้วยศูนย์

```
Input: "10 / 0"
```
- visit BinaryOp(Div) → left_val = 10, right_val = 0
- `right_val == 0` → **throw MathError("division by zero")**

### Parse error

```
Input: "2 + + 3"
```
- Parser เจอ `+` ซ้อนกัน → **throw ParseError("unexpected input")**
