# `factor` Command — Walkthrough แบบละเอียด

## สารบัญ
1. [ภาพรวม (Overview)](#1-ภาพรวม)
2. [Step 1 — Parse expression เป็น AST แล้วแปลงเป็น Polynomial](#step-1--parse-expression-เป็น-ast-แล้วแปลงเป็น-polynomial)
3. [Step 2 — Extract Common Monomial Factor (GCD ของ monomial)](#step-2--extract-common-monomial-factor)
4. [Step 3 — Extract Coefficient GCD + Normalize Sign](#step-3--extract-coefficient-gcd--normalize-sign)
5. [Step 4 — Quadratic Factorization (แยกตัวประกอบกำลังสอง)](#step-4--quadratic-factorization)
6. [Step 5 — สร้าง FactoredForm และแสดงผล](#step-5--สร้าง-factoredform-และแสดงผล)

---

## 1. ภาพรวม

เมื่อผู้ใช้พิมพ์ `factor 2x^2 + 4x + 2` โปรแกรมจะเรียก `cmd_factor("2x^2 + 4x + 2")` ซึ่งอยู่ใน `factor.hpp` ฟังก์ชันนี้ทำงานดังนี้:

```
User Input: "factor 2x^2 + 4x + 2"
  │
  ├─ Step 1: Parser + ASTToPolynomial ──→ Polynomial { {x:2}:2, {x:1}:4, {}:2 }
  ├─ Step 2: monomial_gcd()           ──→ Extract common monomial (ถ้ามี)
  ├─ Step 3: coefficient_gcd()        ──→ Extract GCD ของ coefficients + normalize sign
  ├─ Step 4: try_factor_quadratic()   ──→ Factor quadratic ax^2+bx+c → (px+q)(rx+s)
  └─ Step 5: FactoredForm::to_string()──→ "2(x + 1)^2"
```

> [!NOTE]
> factor ทำงานเป็น **pipeline ของ extraction steps** — ดึง common factor ออกทีละชั้น จนเหลือ polynomial ที่ลดรูปแล้ว แล้วค่อยพยายามแยกตัวประกอบเพิ่ม

---

## Step 1 — Parse expression เป็น AST แล้วแปลงเป็น Polynomial

```cpp
// factor.hpp — cmd_factor()
Parser          parser(args);                    // args = "2x^2 + 4x + 2"
auto            expr = parser.parse();           // ◀── parse เป็น AST

ASTToPolynomial converter(args);
Polynomial      poly = converter.convert(*expr); // ◀── แปลง AST → Polynomial
```

### AST ที่ได้:

```
BinaryOp(Add)
├── left:  BinaryOp(Add)
│           ├── left:  BinaryOp(Mul)
│           │           ├── left:  Number(2)
│           │           └── right: BinaryOp(Pow)
│           │                       ├── left:  Variable("x")
│           │                       └── right: Number(2)
│           └── right: BinaryOp(Mul)
│                       ├── left:  Number(4)
│                       └── right: Variable("x")
└── right: Number(2)
```

### Polynomial ที่ได้หลังแปลง:

```
poly.terms_ = {
    Monomial({x:2}) : 2.0,    ← 2x^2
    Monomial({x:1}) : 4.0,    ← 4x
    Monomial({})    : 2.0     ← 2
}
```

> ขั้นตอนการแปลง AST → Polynomial ใช้ `ASTToPolynomial` (Visitor Pattern) เหมือนกับ `expand` — ดูรายละเอียดที่ [`expand_command.md` Step 2](expand_command.md#step-2--แปลง-ast-เป็น-polynomial)

---

## Step 2 — Extract Common Monomial Factor

```cpp
// factor.hpp — factor_polynomial()
Monomial common_mono = working.monomial_gcd();   // ◀── หา GCD ของ monomials ทุก term
if (!common_mono.is_constant()) {
    working = working.divide_by_monomial(common_mono);
    result.common_monomial = common_mono;
}
```

### 2.1 — `monomial_gcd()` ทำงานอย่างไร?

หาตัว **monomial ที่หารทุก term ลงตัว** โดยเก็บ exponent ต่ำสุดของแต่ละตัวแปร:

```cpp
// polynomial.hpp — monomial_gcd()
Monomial monomial_gcd() const {
    auto it = terms_.begin();
    map<string, int> common = it->first.vars();  // เริ่มจาก term แรก
    ++it;

    for (; it != terms_.end(); ++it) {
        const auto& vars = it->first.vars();
        for (auto cit = common.begin(); cit != common.end();) {
            auto vit = vars.find(cit->first);
            if (vit == vars.end()) {
                cit = common.erase(cit);         // ◀── ตัวแปรนี้ไม่มีใน term นี้ → ลบออก
            } else {
                cit->second = min(cit->second, vit->second);  // ◀── เก็บ exponent ต่ำสุด
                ++cit;
            }
        }
    }
    return Monomial(common);
}
```

**ตัวอย่างสำหรับ `2x^2 + 4x + 2`:**

| Iteration | Term Monomial | common (สะสม) | หมายเหตุ |
|-----------|---------------|---------------|---------|
| เริ่มต้น | `{x:2}` | `{x:2}` | copy จาก term แรก |
| term 2 | `{x:1}` | `{x:1}` | x มีทั้งคู่ → min(2,1) = 1 |
| term 3 | `{}` | `{}` ← ว่าง! | x ไม่มีใน term นี้ → ลบ x ออก |

**ผลลัพธ์:** `common_mono = Monomial{}` — เป็น constant (ว่าง) → ข้าม step นี้

**ตัวอย่างที่ monomial_gcd ทำงานจริง: `factor 4x^2 + 8x`**

| Iteration | Term Monomial | common (สะสม) |
|-----------|---------------|---------------|
| เริ่มต้น | `{x:2}` | `{x:2}` |
| term 2 | `{x:1}` | `{x:1}` ← min(2,1)=1 |

ผลลัพธ์: `common_mono = {x:1}` → **ดึง x ออกมา**

```
working = working.divide_by_monomial({x:1})
ก่อน: { {x:2}: 4,  {x:1}: 8 }     ← 4x^2 + 8x
หลัง: { {x:1}: 4,  {}:    8 }     ← 4x + 8
```

> [!TIP]
> สำหรับ multivariate เช่น `x^2*y - 3*x*y` → monomial GCD = `{x:1, y:1}` = `xy`
> หลังหาร: `x - 3` → ผลลัพธ์สุดท้าย: `xy(x - 3)`

---

## Step 3 — Extract Coefficient GCD + Normalize Sign

### 3.1 — GCD ของ coefficients

```cpp
// factor.hpp — factor_polynomial()
double coeff_gcd = working.coefficient_gcd();   // ◀── หา GCD ของ |coefficients| ทุก term
if (coeff_gcd > 1.0 + 1e-9) {
    working = working / coeff_gcd;               // หารทุก coefficient ด้วย GCD
    result.numeric_factor = coeff_gcd;
}
```

**ตัวอย่าง: `2x^2 + 4x + 2`** (ยังไม่ได้ถูกแก้ไขจาก Step 2):

```cpp
// polynomial.hpp — coefficient_gcd()
double coefficient_gcd() const {
    // ตรวจว่าทุก coefficient เป็นจำนวนเต็ม
    for (auto& [_, c] : terms_)
        if (abs(c - round(c)) > 1e-9) return 1.0;  // ← ไม่ใช่จำนวนเต็ม → return 1

    // หา GCD: gcd(|2|, |4|, |2|)
    int64_t g = 0;
    for (auto& [_, c] : terms_) {
        int64_t ic = static_cast<int64_t>(round(abs(c)));
        g = std::gcd(g, ic);                        // gcd(0,2)=2, gcd(2,4)=2, gcd(2,2)=2
    }
    return static_cast<double>(g);                   // return 2.0
}
```

**ผลลัพธ์:**
```
coeff_gcd = 2.0 > 1.0 ✓
working = working / 2.0
ก่อน: { {x:2}: 2,  {x:1}: 4,  {}: 2 }     ← 2x^2 + 4x + 2
หลัง: { {x:2}: 1,  {x:1}: 2,  {}: 1 }     ← x^2 + 2x + 1

result.numeric_factor = 2.0
```

### 3.2 — Normalize sign

```cpp
// factor.hpp — factor_polynomial()
if (!working.terms().empty()) {
    auto first_coeff = working.terms().begin()->second;  // coefficient ของ term degree สูงสุด
    if (first_coeff < -1e-9) {
        working = -working;                   // กลับเครื่องหมายทุก term
        result.numeric_factor *= -1;          // เก็บ -1 ไว้ใน numeric_factor
    }
}
```

สำหรับ `x^2 + 2x + 1`: leading coeff = 1 > 0 → **ข้าม**

> [!NOTE]
> ทำ normalize sign เพื่อให้ quadratic factoring ทำงานกับ polynomial ที่ leading coefficient เป็นบวกเสมอ
> เช่น `-x^2 + 5x - 6` จะถูกแปลงเป็น `-(x^2 - 5x + 6)` ก่อนโยนเข้า quadratic factoring

---

## Step 4 — Quadratic Factorization

```cpp
// factor.hpp — factor_polynomial()
if (working.is_univariate() && working.degree() == 2) {
    std::string var = working.single_variable();       // var = "x"
    std::vector<std::pair<Polynomial, int>> quad_factors;
    if (try_factor_quadratic(working, var, quad_factors)) {
        result.factors = quad_factors;
        return result;
    }
}
```

### 4.1 — เงื่อนไขเข้า quadratic factoring

- `is_univariate()` → ตัวแปรเดียว? ✓ (มีแค่ `x`)
- `degree() == 2` → degree สูงสุด = 2? ✓

### 4.2 — `try_factor_quadratic()` ทำงานอย่างไร?

```cpp
// factor.hpp — try_factor_quadratic()
inline bool try_factor_quadratic(const Polynomial& poly, const string& var,
                                 vector<pair<Polynomial, int>>& factors) {
    double a = poly.coeff_of_degree(2);    // a = 1 (coefficient ของ x^2)
    double b = poly.coeff_of_degree(1);    // b = 2 (coefficient ของ x)
    double c = poly.coeff_of_degree(0);    // c = 1 (constant term)
```

**สำหรับ `x^2 + 2x + 1`:** `a=1, b=2, c=1`

### 4.3 — คำนวณ Discriminant

```cpp
    int64_t ia = 1, ib = 2, ic = 1;

    int64_t disc = ib * ib - 4 * ia * ic;   // disc = 4 - 4 = 0
    if (disc < 0) return false;              // disc >= 0 ✓

    int64_t sqrt_disc = round(sqrt(0));      // sqrt_disc = 0
    if (sqrt_disc * sqrt_disc != disc)       // 0*0 == 0 ✓ (perfect square)
        return false;
```

> [!IMPORTANT]
> **Discriminant = 0** หมายความว่า polynomial มี **repeated root** (perfect square trinomial)
> - disc > 0 → 2 distinct real roots
> - disc = 0 → 1 repeated root (perfect square)
> - disc < 0 → no real roots → ไม่สามารถ factor over integers ได้

### 4.4 — Brute-force หา factor pairs

เป้าหมาย: หา `p, q, r, s` ที่ `(px + q)(rx + s) = ax^2 + bx + c`

เงื่อนไข:
- `p * r = a` (ผลคูณ leading coefficients = a)
- `q * s = c` (ผลคูณ constants = c)
- `p * s + q * r = b` (cross products = b)

```cpp
    // หา divisors ของ |a| และ |c|
    auto divs_a = get_divisors(1);    // divs_a = {1}
    auto divs_c = get_divisors(1);    // divs_c = {1}

    // วนทุกคู่ divisors พร้อมทุก sign combination
    for (int64_t p : divs_a) {        // p = 1
        int64_t r = ia / p;           // r = 1/1 = 1
        for (int64_t q : divs_c) {    // q = 1
            int64_t s = ic / q;       // s = 1/1 = 1
            // Try signs: (sp, sq, sr, ss) ∈ {+1, -1}^4
            // ...
            // p=1, q=1, r=1, s=1:
            //   p*r=1 == ia(1) ✓
            //   q*s=1 == ic(1) ✓
            //   p*s + q*r = 1+1 = 2 == ib(2) ✓  ← ✓ FOUND!
        }
    }
```

**ผลลัพธ์:** `best_p=1, best_q=1, best_r=1, best_s=1`

### 4.5 — สร้าง factor polynomials

```cpp
    // (px + q) = (1·x + 1) = x + 1
    Polynomial f1(1.0, "x", 1);   // = x
    f1 = f1 + Polynomial(1.0);    // = x + 1

    // (rx + s) = (1·x + 1) = x + 1
    Polynomial f2(1.0, "x", 1);   // = x
    f2 = f2 + Polynomial(1.0);    // = x + 1

    // f1 == f2 → perfect square!
    if (f1.to_string() == f2.to_string()) {
        factors.push_back({f1, 2});    // ◀── (x + 1)^2
    }
```

**ผลลัพธ์:** `factors = [(Polynomial("x + 1"), 2)]` → `(x + 1)^2`

---

## Step 5 — สร้าง FactoredForm และแสดงผล

### 5.1 — FactoredForm struct

```
FactoredForm
├── numeric_factor:  2.0              ← จาก Step 3 (coefficient GCD)
├── common_monomial: Monomial{}       ← ว่าง (ไม่มี common monomial)
└── factors:
    └── (Polynomial("x + 1"), exp: 2) ← จาก Step 4
```

### 5.2 — `to_string()` ทำงานอย่างไร?

```cpp
// factor.hpp — FactoredForm::to_string()
std::string to_string() const {
    // ตรวจก่อนว่า irreducible หรือเปล่า (ไม่ได้ factor อะไรเลย)
    if (is_trivial()) {
        return factors[0].first.to_string();   // ← แสดงแค่ polynomial ปกติ ไม่ใส่วงเล็บ
    }

    std::string result;

    // Numeric coefficient
    // numeric_factor = 2.0, abs(2 - 1) > ε ✓ → has_numeric = true
    // is_negative = false
    result += "2";                              // ← แสดง "2"

    // Common monomial
    // common_monomial = {} → empty string → ข้าม

    // Factors
    for (auto& [poly, exp] : factors) {
        result += "(" + poly.to_string() + ")"; // ← "(x + 1)"
        if (exp > 1)
            result += "^" + to_string(exp);     // ← "^2"
    }

    return result;   // "2(x + 1)^2"
}
```

**ผลลัพธ์ที่ผู้ใช้เห็น:**
```
  2(x + 1)^2
```

---

## ตัวอย่างเพิ่มเติม

### ตัวอย่าง 1: `factor x^2 - 5x + 6`

| Step | ค่าสำคัญ |
|------|---------|
| **Polynomial** | `{ {x:2}: 1, {x:1}: -5, {}: 6 }` → `x^2 - 5x + 6` |
| **Monomial GCD** | `{}` (constant term มี `{}`) → ข้าม |
| **Coeff GCD** | `gcd(1, 5, 6)` = 1 → ข้าม |
| **Quadratic** | `a=1, b=-5, c=6` |
| **Discriminant** | `25 - 24 = 1` → `√1 = 1` ✓ |
| **Factor pairs** | `p=1,q=-3,r=1,s=-2`: `1·(-2) + (-3)·1 = -5` ✓ |
| **Output** | `(x - 3)(x - 2)` |

### ตัวอย่าง 2: `factor x^2 - 9` (Difference of Squares)

| Step | ค่าสำคัญ |
|------|---------|
| **Polynomial** | `{ {x:2}: 1, {}: -9 }` → `x^2 - 9` (ไม่มี term x^1) |
| **Quadratic** | `a=1, b=0, c=-9` |
| **Discriminant** | `0 - 4·1·(-9) = 36` → `√36 = 6` ✓ |
| **Factor pairs** | `p=1,q=-3,r=1,s=3`: `1·3 + (-3)·1 = 0` ✓ |
| **Output** | `(x - 3)(x + 3)` |

> [!TIP]
> difference of squares `a^2 - b^2 = (a-b)(a+b)` ถูกจัดการโดย quadratic factoring โดยอัตโนมัติ เพราะ `b=0, c=-b^2` จะมี discriminant เป็น perfect square เสมอ

### ตัวอย่าง 3: `factor 4x^2 + 8x` (Common Factor Extraction)

| Step | ค่าสำคัญ |
|------|---------|
| **Polynomial** | `{ {x:2}: 4, {x:1}: 8 }` → `4x^2 + 8x` |
| **Monomial GCD** | `{x:1}` → ดึง `x` ออก → `working = 4x + 8` |
| **Coeff GCD** | `gcd(4, 8)` = 4 → ดึง `4` ออก → `working = x + 2` |
| **Degree check** | degree = 1 → **ข้าม** quadratic factoring |
| **FactoredForm** | `numeric_factor=4, common_monomial={x:1}, factors=[(x+2, 1)]` |
| **Output** | `4x(x + 2)` |

### ตัวอย่าง 4: `factor 2x^2 + 7x + 3` (Leading coeff > 1)

| Step | ค่าสำคัญ |
|------|---------|
| **Polynomial** | `{ {x:2}: 2, {x:1}: 7, {}: 3 }` |
| **Coeff GCD** | `gcd(2, 7, 3)` = 1 → ข้าม |
| **Quadratic** | `a=2, b=7, c=3` |
| **Discriminant** | `49 - 24 = 25` → `√25 = 5` ✓ |
| **Factor pairs** | `divs_a={1,2}, divs_c={1,3}` |
| | `p=1,r=2, q=3,s=1`: `1·1 + 3·2 = 7` ✓ |
| **Output** | `(x + 3)(2x + 1)` |

### ตัวอย่าง 5: `factor x^2 + x + 1` (Irreducible)

| Step | ค่าสำคัญ |
|------|---------|
| **Quadratic** | `a=1, b=1, c=1` |
| **Discriminant** | `1 - 4 = -3` → **disc < 0** → return false |
| **Irreducible** | `FactoredForm.is_trivial()` = true → แสดงไม่ใส่วงเล็บ |
| **Output** | `x^2 + x + 1` |

> [!IMPORTANT]
> ถ้า **discriminant < 0** polynomial ไม่สามารถแยกตัวประกอบเป็นจำนวนจริงได้ → แสดงคืนค่าเดิมโดยไม่ใส่วงเล็บ

---

## Internal Data Structures

### FactoredForm

```
FactoredForm
├── numeric_factor: double              ← ตัวเลขที่คูณอยู่ข้างหน้า (เช่น 2 ใน 2(x+1)^2)
├── common_monomial: Monomial           ← monomial ที่ดึงออกมา (เช่น xy ใน xy(x-3))
└── factors: vector<pair<Poly, int>>    ← รายการ (polynomial, exponent)
                                           เช่น [(x+1, 2)] = (x+1)^2
```

**ตัวอย่างต่างๆ:**

| Input | numeric_factor | common_monomial | factors |
|-------|----------------|-----------------|---------|
| `2(x+1)^2` | 2.0 | `{}` | `[(x+1, 2)]` |
| `4x(x+2)` | 4.0 | `{x:1}` | `[(x+2, 1)]` |
| `xy(x-3)` | 1.0 | `{x:1, y:1}` | `[(x-3, 1)]` |
| `(x-3)(x+3)` | 1.0 | `{}` | `[(x-3, 1), (x+3, 1)]` |
| `x^2+x+1` | 1.0 | `{}` | `[(x^2+x+1, 1)]` ← trivial |

---

## Error Cases

### กรณี non-polynomial input

```
factor x / (x + 1)
```
- `ASTToPolynomial` เจอ division by variable expression
- **Throw `PolynomialError`**: `"cannot divide by a variable expression"`

### กรณี fractional coefficients

```
factor 0.5x^2 + 1.5x + 1
```
- `coefficient_gcd()` ตรวจว่า coefficients ไม่ใช่จำนวนเต็ม → return `1.0`
- Quadratic factoring: `a=0.5, b=1.5, c=1` → ไม่ใช่จำนวนเต็ม → return false
- ผลลัพธ์: แสดงคืนค่าเดิม `0.5x^2 + 1.5x + 1`

### กรณี higher degree (degree > 2)

```
factor x^3 + 3x^2 + 3x + 1
```
- `working.degree() == 3` → **ข้ามขั้น quadratic factoring** (รองรับแค่ degree 2)
- ผลลัพธ์: แสดงคืนค่าเดิม (ปัจจุบันยังไม่รองรับ cubic factoring)

> [!NOTE]
> **ข้อจำกัดปัจจุบัน:**
> - รองรับ quadratic (degree 2) เท่านั้น
> - Factor เฉพาะ **univariate** quadratics (ตัวแปรเดียว)
> - ใช้ได้เฉพาะ **integer coefficients** ในการหา factor pairs
> - **ตัวแปรหลายตัวอักษร**: `xy` ถูก Lexer มองเป็น identifier เดียว ต้องใช้ `x*y` คั่น
