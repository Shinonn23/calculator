# `expand` Command — Walkthrough แบบละเอียด

## สารบัญ
1. [ภาพรวม (Overview)](#1-ภาพรวม)
2. [Step 1 — Parse expression เป็น AST](#step-1--parse-expression-เป็น-ast)
3. [Step 2 — แปลง AST เป็น Polynomial (ASTToPolynomial)](#step-2--แปลง-ast-เป็น-polynomial)
4. [Step 3 — Polynomial Arithmetic (กระจายพจน์ + รวม like terms)](#step-3--polynomial-arithmetic)
5. [Step 4 — จัดเรียง Canonical Form และแสดงผล](#step-4--จัดเรียง-canonical-form-และแสดงผล)

---

## 1. ภาพรวม

เมื่อผู้ใช้พิมพ์ `expand (x+2)^3` โปรแกรมจะเรียก `cmd_expand("(x+2)^3")` ซึ่งอยู่ใน `expand.hpp` ฟังก์ชันนี้ทำงานเป็น 4 ขั้นตอนดังนี้:

```
User Input: "expand (x+2)^3"
  │
  ├─ Step 1: Parser          ──→  AST (BinaryOp tree)
  ├─ Step 2: ASTToPolynomial ──→  Polynomial (map<Monomial, double>)
  ├─ Step 3: Arithmetic      ──→  กระจายพจน์ + รวม like terms อัตโนมัติ
  └─ Step 4: to_string()     ──→  "x^3 + 6x^2 + 12x + 8"
```

> [!NOTE]
> `expand` ไม่ต้องการ Context หรือตัวแปรจากผู้ใช้ — มันทำงานเป็น pure symbolic manipulation ทั้งหมด

---

## Step 1 — Parse expression เป็น AST

```cpp
// expand.hpp — cmd_expand()
Parser parser(args);            // args = "(x+2)^3"
auto   expr = parser.parse();   // ◀── parse เป็น AST
```

### เกิดอะไรขึ้นข้างใน `Parser`?

**1.1** Constructor `Parser("(x+2)^3")` สร้าง `Lexer` ซึ่งจะ **tokenize** string ออกมาเป็น token list:

```
Token stream:
  LParen, Variable(x), Plus, Number(2), RParen, Pow, Number(3), End
```

**1.2** เรียก `parse()` → `parse_expression()` → `parse_additive()` → `parse_multiplicative()` → `parse_power()`

ลำดับการ parse ใน `parse_power()`:

1. `parse_unary()` → `parse_primary()` → เจอ `LParen` → เข้า parse ข้างในวงเล็บ
   - `parse_expression()` ภายในวงเล็บ → parse `x + 2`
   - `parse_primary()`: `Variable(x)` → return `Variable("x")`
   - กลับที่ `parse_additive()`: เจอ `Plus` → อ่าน `Number(2)`
   - สร้าง `BinaryOp(Variable("x"), Add, Number(2))`
   - เจอ `RParen` → กินออก → return subtree
2. กลับมาที่ `parse_power()` → เจอ token `Pow` (`^`)
3. `advance()` → อ่าน `parse_unary()` → `Number(3)`
4. สร้าง `BinaryOp(subtree, Pow, Number(3))`

**ผลลัพธ์ AST:**

```
BinaryOp(Pow)
├── left:  BinaryOp(Add)
│           ├── left:  Variable("x")
│           └── right: Number(2)
└── right: Number(3)
```

> [!TIP]
> AST ยังเป็นโครงสร้างแบบ symbolic ไม่ได้ "expand" อะไรเลย — การ expand จริงเกิดที่ Step 2

---

## Step 2 — แปลง AST เป็น Polynomial

```cpp
// expand.hpp — cmd_expand()
ASTToPolynomial converter(args);                  // สร้าง converter
Polynomial      poly = converter.convert(*expr);  // ◀── เดิน AST → สร้าง Polynomial
```

### เกิดอะไรขึ้นข้างใน `ASTToPolynomial`?

`ASTToPolynomial` implement **Visitor Pattern** — เมื่อเรียก `convert()` ตัว converter จะ **เดิน AST แบบ depth-first** แล้วสร้าง `Polynomial` object ขึ้นมาทีละ node:

```cpp
// ast_to_poly.hpp
Polynomial convert(const Expr& expr) {
    expr.accept(*this);     // ◀── Visitor dispatch
    return result_;
}
```

### 2.1 — Data Structures ที่สำคัญ

**Monomial** — แทน product ของตัวแปรยกกำลัง เช่น `x^2y` = `{x:2, y:1}`:

```
Monomial
├── vars_: map<string, int>
│          เช่น {"x": 2, "y": 1}
├── total_degree() → ผลรวม exponents (เช่น 2+1 = 3)
└── operator*()    → คูณ monomial: บวก exponents ทีละตัว
```

**Polynomial** — แทนผลรวมของ (coefficient × monomial):

```
Polynomial
├── terms_: map<Monomial, double>
│           เช่น { {x:2}: 3.0, {x:1}: -5.0, {}: 7.0 }
│           แปลว่า 3x^2 - 5x + 7
├── operator+()  → บวก polynomial: รวม coefficients ของ monomial เดียวกัน
├── operator*()  → คูณ polynomial: กระจายพจน์ (distributive law)
└── pow(n)       → ยกกำลัง: fast exponentiation (O(log n))
```

> [!IMPORTANT]
> `map<Monomial, double>` ใช้ `Monomial::operator<` เป็น key → terms ที่มี monomial เหมือนกันจะ **ถูกรวมอัตโนมัติ** (like terms combining) เมื่อ `+=` ค่า coefficient เข้าไป

### 2.2 — ลำดับการ visit (depth-first traversal)

สำหรับ AST ของ `(x+2)^3`:

```
visit BinaryOp(Pow)                          ← root
 │
 ├── [convert left subtree]
 │    visit BinaryOp(Add)                    ← (x + 2)
 │     ├── visit Variable("x")
 │     │     → result_ = Polynomial(1.0, "x", 1)
 │     │       terms_ = { {x:1}: 1.0 }       ← แปลว่า "1·x"
 │     │
 │     ├── left = { {x:1}: 1.0 }
 │     │
 │     ├── visit Number(2)
 │     │     → result_ = Polynomial(2.0)
 │     │       terms_ = { {}: 2.0 }           ← แปลว่า "2" (constant)
 │     │
 │     ├── right = { {}: 2.0 }
 │     │
 │     └── op = Add
 │         → result_ = left + right
 │                    = { {x:1}: 1.0,  {}: 2.0 }
 │                    ← แปลว่า "x + 2"
 │
 ├── left = Polynomial("x + 2")
 │
 ├── [convert right subtree]
 │    visit Number(3)
 │     → result_ = Polynomial(3.0)
 │       terms_ = { {}: 3.0 }                ← แปลว่า "3"
 │
 ├── right = Polynomial(3.0)
 │
 └── op = Pow
     → ตรวจ: right.is_constant()? ✓  (เป็นค่าคงที่)
     → exp_val = 3.0
     → exp_int = 3
     → ตรวจ: exp_int >= 0 และ เป็นจำนวนเต็ม? ✓
     → result_ = left.pow(3)
```

> [!NOTE]
> `ASTToPolynomial` สร้าง converter ใหม่ (`ASTToPolynomial left_conv`, `right_conv`) สำหรับ subtree แต่ละข้าง เพื่อไม่ให้ `result_` ของ parent ถูก overwrite ระหว่าง recursion

---

## Step 3 — Polynomial Arithmetic (กระจายพจน์ + รวม like terms)

### 3.1 — `pow(3)` ทำงานอย่างไร?

```cpp
// polynomial.hpp — pow()
Polynomial pow(int n) const {
    // Fast exponentiation: O(log n) multiplications
    Polynomial result(1);        // result = 1
    Polynomial base = *this;     // base = (x + 2)
    int        exp  = 3;

    while (exp > 0) {
        if (exp % 2 == 1)        // ถ้า exp เป็นเลขคี่
            result = result * base;
        base = base * base;
        exp /= 2;
    }
    return result;
}
```

**Trace ทีละรอบ:**

| Iteration | exp | exp%2 | result                | base                       |
| --------- | --- | ----- | --------------------- | -------------------------- |
| เริ่มต้น     | 3   | —     | `1`                   | `x + 2`                    |
| 1         | 3   | 1 (คี่) | `1 * (x+2)` = `x + 2` | `(x+2)^2` = `x^2 + 4x + 4` |
| 2         | 1   | 1 (คี่) | `(x+2) * (x^2+4x+4)`  | ... (ไม่ใช้แล้ว)              |
| 3         | 0   | —     | **done**              |                            |

### 3.2 — Polynomial multiplication (`operator*`) ทำงานอย่างไร?

```cpp
// polynomial.hpp — operator*()
Polynomial operator*(const Polynomial& other) const {
    Polynomial result;
    for (const auto& [m1, c1] : terms_) {          // ◀── วนทุก term ของ this
        for (const auto& [m2, c2] : other.terms_) {// ◀── วนทุก term ของ other
            Monomial product = m1 * m2;             // คูณ monomial (บวก exponents)
            result.terms_[product] += c1 * c2;      // สะสม coefficient
        }                                           // ⚡ like terms รวมอัตโนมัติ!
    }
    result.cleanup();   // ลบ terms ที่ coefficient ≈ 0
    return result;
}
```

**ตัวอย่าง: `(x + 2) * (x^2 + 4x + 4)`**

ทีนี้ `this` = `{  {x:1}: 1,  {}: 2  }` และ `other` = `{  {x:2}: 1,  {x:1}: 4,  {}: 4  }`

| m1 (this) | c1  | ×   | m2 (other) | c2  | →   | product monomial | c1×c2 | สะสมใน result                       |
| --------- | --- | --- | ---------- | --- | --- | ---------------- | ----- | ----------------------------------- |
| `{x:1}`   | 1   | ×   | `{x:2}`    | 1   | →   | `{x:3}`          | 1     | `{x:3}: 1`                          |
| `{x:1}`   | 1   | ×   | `{x:1}`    | 4   | →   | `{x:2}`          | 4     | `{x:2}: 4`                          |
| `{x:1}`   | 1   | ×   | `{}`       | 4   | →   | `{x:1}`          | 4     | `{x:1}: 4`                          |
| `{}`      | 2   | ×   | `{x:2}`    | 1   | →   | `{x:2}`          | 2     | `{x:2}: 4+2 = 6` ← **like terms!**  |
| `{}`      | 2   | ×   | `{x:1}`    | 4   | →   | `{x:1}`          | 8     | `{x:1}: 4+8 = 12` ← **like terms!** |
| `{}`      | 2   | ×   | `{}`       | 4   | →   | `{}`             | 8     | `{}: 8`                             |

**ผลลัพธ์สุดท้าย:**
```
terms_ = {
    {x:3}: 1,     ← x^3
    {x:2}: 6,     ← 6x^2
    {x:1}: 12,    ← 12x
    {}:    8      ← 8
}
```

> [!IMPORTANT]
> **Like terms combining** เกิดขึ้นอัตโนมัติเพราะ `result.terms_[product] += c1 * c2` — ถ้า `product` monomial ซ้ำกัน (map key เดิม) ค่า coefficient จะถูกบวกสะสมเข้าด้วยกัน ไม่ต้องทำ pass เพิ่ม

---

## Step 4 — จัดเรียง Canonical Form และแสดงผล

```cpp
// expand.hpp — cmd_expand()
std::cout << "  " << poly.to_string() << "\n";
```

### 4.1 — Monomial ordering (Graded Lexicographic)

Terms ใน `map<Monomial, double>` ถูกจัดเรียงโดย `Monomial::operator<` แบบ **Graded Lexicographic Order**:

```cpp
// polynomial.hpp — Monomial::operator<()
bool operator<(const Monomial& other) const {
    int d1 = total_degree();
    int d2 = other.total_degree();
    if (d1 != d2)
        return d1 > d2;                // ◀── degree สูงกว่ามาก่อน

    // degree เท่ากัน: เทียบ exponent ของตัวแปรตามลำดับ alphabet
    for (const auto& v : all_vars) {
        int e1 = degree_of(v);
        int e2 = other.degree_of(v);
        if (e1 != e2)
            return e1 > e2;            // ◀── exponent สูงกว่าของตัวแปรแรกมาก่อน
    }
    return false;
}
```

**กฎ:**
1. **Total degree สูงกว่ามาก่อน** — เช่น `x^3` (degree 3) มาก่อน `x^2` (degree 2)
2. **Degree เท่ากัน** → เรียงตาม exponent ของตัวแปรตัวแรก (alphabetically) — เช่น `x^2` มาก่อน `xy` มาก่อน `y^2` (ทุกตัว degree 2)

**ตัวอย่างลำดับสำหรับ multivariate `(x+y)^2`:**

```
x^2   → total_degree=2, x_exp=2, y_exp=0  ← มาก่อน
xy    → total_degree=2, x_exp=1, y_exp=1  ← กลาง (x_exp น้อยกว่า x^2)
y^2   → total_degree=2, x_exp=0, y_exp=2  ← มาหลัง (x_exp น้อยสุด)
```

ผลลัพธ์: `x^2 + 2xy + y^2` ✓

### 4.2 — Polynomial::to_string()

```cpp
// polynomial.hpp — to_string()
std::string to_string() const {
    std::string result;
    bool first = true;

    for (const auto& [m, c] : terms_) {      // ◀── วน terms ตาม ordering
        std::string mono_str = m.to_string(); // เช่น "x^3", "x^2", "x", ""

        if (first) {
            // term แรก: ถ้า coeff = 1 ไม่ต้องแสดง, ถ้า -1 แสดง "-"
            if (has_vars && abs(c - 1.0) < ε)
                result += mono_str;           // "x^3"
            else if (has_vars && abs(c + 1.0) < ε)
                result += "-" + mono_str;     // "-x^3"
            else
                result += format_number(c) + mono_str;  // "6x^2"
        } else {
            // term ถัดไป: ใส่ " + " หรือ " - "
            if (c > 0) result += " + " + ...;
            else       result += " - " + ...;
        }
    }
}
```

**สำหรับ `(x+2)^3`:**

| Term | Monomial | Coeff | แสดงผล             |
| ---- | -------- | ----- | ------------------ |
| 1st  | `{x:3}`  | 1     | `x^3` (ไม่แสดง "1") |
| 2nd  | `{x:2}`  | 6     | ` + 6x^2`          |
| 3rd  | `{x:1}`  | 12    | ` + 12x`           |
| 4th  | `{}`     | 8     | ` + 8` (constant)  |

**ผลลัพธ์ที่ผู้ใช้เห็น:**
```
  x^3 + 6x^2 + 12x + 8
```

---

## ตัวอย่างเพิ่มเติม

### Implicit Multiplication: `expand 2(x+3)^2`

| Step              | ค่าสำคัญ                                                                                |
| ----------------- | ------------------------------------------------------------------------------------ |
| **Tokenize**      | `Number(2), LParen, Variable(x), Plus, Number(3), RParen, Pow, Number(2)`            |
| **Parse**         | `2` → implicit mul → `(x+3)^2` → AST: `BinaryOp(Mul, Number(2), BinaryOp(Pow, ...))` |
| **Convert left**  | `Polynomial(2.0)` = `{ {}: 2 }`                                                      |
| **Convert right** | `(x+3).pow(2)` = `x^2 + 6x + 9` = `{ {x:2}: 1, {x:1}: 6, {}: 9 }`                    |
| **Multiply**      | `2 * (x^2 + 6x + 9)` = `{ {x:2}: 2, {x:1}: 12, {}: 18 }`                             |
| **Output**        | `2x^2 + 12x + 18`                                                                    |

### Chained Multiply: `expand (x+1)(x-1)(x+2)`

| Step       | ค่าสำคัญ                                                                               |
| ---------- | ----------------------------------------------------------------------------------- |
| **Parse**  | Implicit mul chain: `((x+1) * (x-1)) * (x+2)`                                       |
| **Step A** | `(x+1) * (x-1)` = `x^2 - 1` ← difference of squares (computed via distributive law) |
| **Step B** | `(x^2 - 1) * (x+2)` = `x^3 + 2x^2 - x - 2`                                          |
| **Output** | `x^3 + 2x^2 - x - 2`                                                                |

### Multivariate: `expand (x+y)^2`

| Step             | ค่าสำคัญ                                              |
| ---------------- | -------------------------------------------------- |
| **Convert**      | left = `{x:1}: 1 + {y:1}: 1` → Polynomial(x + y)   |
| **pow(2)**       | `(x+y) * (x+y)` → กระจายพจน์ 4 คู่                    |
| **Distributive** | `x·x + x·y + y·x + y·y` = `x^2 + xy + xy + y^2`    |
| **Like terms**   | `xy` รวมกัน → `2xy`                                 |
| **Ordering**     | `x^2` (x_exp=2) > `xy` (x_exp=1) > `y^2` (x_exp=0) |
| **Output**       | `x^2 + 2xy + y^2`                                  |

---

## Error Cases

### กรณี division by variable

```
expand x / (x + 1)
```
- `ASTToPolynomial` เจอ `BinaryOp(Div)` → ตรวจ `right.is_constant()`? → **No**
- **Throw `PolynomialError`**: `"cannot divide by a variable expression"`

### กรณี fractional exponent

```
expand x^0.5
```
- `ASTToPolynomial` เจอ `BinaryOp(Pow)` → `exp_val = 0.5`
- ตรวจ: `abs(0.5 - 0) > 1e-9` → exponent ไม่ใช่จำนวนเต็ม
- **Throw `PolynomialError`**: `"exponent must be a non-negative integer"`

### กรณี negative exponent

```
expand x^(-2)
```
- `exp_int = -2`, ตรวจ `exp_int < 0` → true
- **Throw `PolynomialError`**: `"exponent must be a non-negative integer"`

### กรณี constant expression

```
expand (3)^2
```
- `Variable` ไม่ถูกสร้างเลย → ทุก node เป็น `Number`
- ผลลัพธ์: `Polynomial(9.0)` → `to_string()` = `"9"` 

> [!NOTE]
> expand รองรับ **multivariate** (หลายตัวแปร) ได้เต็มที่ เช่น `expand (a+b+c)^2` → `a^2 + 2ab + 2ac + b^2 + 2bc + c^2`
> แต่สำหรับตัวแปรหลายตัวอักษร (เช่น `xy`) Lexer จะมองเป็น **identifier เดียว** ไม่ใช่ `x*y` — ต้องใช้ `*` คั่น: `x*y`
