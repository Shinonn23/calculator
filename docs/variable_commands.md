# `:unset`, `:clear`, `:vars` Commands — Walkthrough แบบละเอียด

## สารบัญ
1. [`:unset` — ลบตัวแปร](#1-unset--ลบตัวแปร)
2. [`:clear` — ลบตัวแปรทั้งหมด](#2-clear--ลบตัวแปรทั้งหมด)
3. [`:vars` — แสดงตัวแปรทั้งหมด](#3-vars--แสดงตัวแปรทั้งหมด)

---

## 1. `:unset` — ลบตัวแปร

### ภาพรวม

```
User Input: ":unset x"
  → dispatch() ตรวจ starts_with(":unset ") → true
  → cmd_unset("x", g_ctx)
```

### Flow ทั้งหมด

```cpp
// command.hpp — cmd_unset()
inline void cmd_unset(const string& args, Context& g_ctx) {
    string var_name = trim(args);           // var_name = "x"

    if (var_name.empty()) {
        cout << "  Usage: :unset <variable>\n";
        return;
    }

    if (g_ctx.unset(var_name)) {            // ◀── พยายามลบ
        cout << "  Removed: " << var_name << "\n";
    } else {
        cout << "  Variable '" << var_name << "' not found\n";
        // ◀── ถ้าหาไม่เจอ → แนะนำชื่อที่ใกล้เคียง
        auto match = suggest(var_name, g_ctx.all_names());
        if (match) {
            cout << "  Did you mean " << *match << "?\n";
        }
    }
}
```

### ข้างใน `Context::unset()`

```cpp
// context.hpp
bool unset(const string& name) {
    return variables_.erase(name) > 0;
    // erase() return จำนวนที่ลบได้
    // ถ้าลบได้ → return true
    // ถ้าไม่มี key นั้น → return false
}
```

### ตัวอย่างทีละขั้นตอน

**สมมติ:** `g_ctx = {x:5, y:3, radius:10}`

```
Input: ":unset x"
```

| Step               | สิ่งที่เกิดขึ้น                                  | `g_ctx` หลังจากนั้น   |
| ------------------ | ----------------------------------------- | ------------------ |
| `trim("x")`        | `var_name = "x"`                          | —                  |
| `g_ctx.unset("x")` | `variables_.erase("x")` → return 1 → true | `{y:3, radius:10}` |
| Output             | `  Removed: x`                            | —                  |

```
Input: ":unset z"    (ไม่มี z ใน context)
```

| Step                           | สิ่งที่เกิดขึ้น                                       |
| ------------------------------ | ---------------------------------------------- |
| `g_ctx.unset("z")`             | `variables_.erase("z")` → return 0 → false     |
| Output                         | `  Variable 'z' not found`                     |
| `suggest("z", ["y","radius"])` | edit_distance("z","y") = 1, ("z","radius") = 5 |
|                                | best = "y" (distance 1 ≤ max 2)                |
| Output                         | `  Did you mean y?`                            |

### `suggest()` — Levenshtein Distance

```cpp
// suggest.hpp
optional<string> suggest(const string& input, const vector<string>& candidates,
                          int max_distance = 2) {
    // วนหา candidate ที่มี edit distance น้อยที่สุด
    // ถ้า distance ≤ max_distance → return candidate
    // ถ้าไม่มีใครใกล้พอ → return nullopt
}
```

**ตัวอย่าง edit_distance:**

| input     | candidate  | distance    | suggest? |
| --------- | ---------- | ----------- | -------- |
| `"raius"` | `"radius"` | 1 (เพิ่ม `d`) | ✅        |
| `"abc"`   | `"xyz"`    | 3           | ❌ (> 2)  |
| `"y"`     | `"x"`      | 1           | ✅        |

---

## 2. `:clear` — ลบตัวแปรทั้งหมด

### ภาพรวม

```
User Input: ":clear"
  → dispatch() ตรวจ input == ":clear" || input == ":cls" → true
  → cmd_clear(g_ctx)
```

### Flow ทั้งหมด

```cpp
// command.hpp — cmd_clear()
inline void cmd_clear(Context& g_ctx) {
    size_t count = g_ctx.size();    // นับจำนวนตัวแปรก่อน
    g_ctx.clear();                   // ลบทั้งหมด
    cout << "  Cleared " << count << " variable(s)\n";
}
```

### ข้างใน `Context::clear()`

```cpp
// context.hpp
void clear() { variables_.clear(); }
// เคลียร์ unordered_map ทั้งหมดทีเดียว
```

### ตัวอย่าง

**สมมติ:** `g_ctx = {x:5, y:3, pi:3.14159}`

```
Input: ":clear"
  → count = g_ctx.size() = 3
  → g_ctx.clear()
  → g_ctx = {}
  → Output: "  Cleared 3 variable(s)"
```

> **⚠️ หมายเหตุ:** `:clear` ลบเฉพาะตัวแปรของ **session ปัจจุบัน** (ใน RAM)
> ถ้าต้องการกู้คืน → ใช้ `:env load default` เพื่อ load จาก config file

---

## 3. `:vars` — แสดงตัวแปรทั้งหมด

### ภาพรวม

```
User Input: ":vars"
  → dispatch() ตรวจ input == ":vars" → true
  → cmd_vars(g_ctx, g_config)
```

### Flow ทั้งหมด

```cpp
// command.hpp — cmd_vars()
inline void cmd_vars(Context& g_ctx, Config& g_config) {
    if (g_ctx.empty()) {
        cout << "  No variables defined\n";
        return;
    }

    // หาความยาวชื่อตัวแปรที่ยาวที่สุด (สำหรับจัด column)
    size_t max_len = 0;
    for (const auto& [name, _] : g_ctx.all())
        max_len = max(max_len, name.size());

    // แสดงทีละตัว พร้อม padding ให้ = อยู่ตำแหน่งเดียวกัน
    for (const auto& [name, value] : g_ctx.all()) {
        cout << "  " << name;
        for (size_t i = name.size(); i < max_len; ++i)
            cout << ' ';                       // padding ด้วย space
        cout << " = " << fmt(value, g_config) << "\n";
    }
}
```

### ตัวอย่าง

**สมมติ:** `g_ctx = {x:5, y:3, radius:10}`, `g_config.precision = 6`

```
max_len = max(1, 1, 6) = 6   ("radius" มีความยาว 6)
```

| name     | `name.size()` | padding spaces       | Output          |
| -------- | ------------- | -------------------- | --------------- |
| `x`      | 1             | 5 spaces (`"     "`) | `  x      = 5`  |
| `y`      | 1             | 5 spaces             | `  y      = 3`  |
| `radius` | 6             | 0 spaces             | `  radius = 10` |

**Output:**
```
  x      = 5
  y      = 3
  radius = 10
```

### `fmt()` — Format ตัวเลข

```cpp
// utils.hpp
inline string fmt(double value, const Config& g_config) {
    return format_double(value, g_config.settings().precision);
}
```

`format_double()` จะ:
1. จัดทศนิยมตาม `precision` (default = 6)
2. ลบ trailing zeros: `"5.000000"` → `"5"`
3. จัดการ `-0` → `"0"`
4. จัดการ NaN/Infinity

**ตัวอย่าง:**

| value     | precision | ผลลัพธ์       |
| --------- | --------- | ----------- |
| `5.0`     | 6         | `"5"`       |
| `3.14159` | 6         | `"3.14159"` |
| `2.5`     | 2         | `"2.5"`     |
| `0.0`     | 6         | `"0"`       |
| `-0.0`    | 6         | `"0"`       |
| `NaN`     | —         | `"NaN"`     |
