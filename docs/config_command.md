# `:config` Command — Walkthrough แบบละเอียด

## สารบัญ
1. [ภาพรวม (Overview)](#1-ภาพรวม)
2. [`:config list` — แสดง settings ทั้งหมด](#config-list)
3. [`:config get <key>` — ดูค่า setting](#config-get)
4. [`:config set <key> <value>` — เปลี่ยน setting](#config-set)
5. [`:config path` — ดู path ของ config file](#config-path)
6. [`:config reset` — reset เป็น default](#config-reset)
7. [Settings ทั้งหมด](#settings-ทั้งหมด)
8. [Config file structure (JSON)](#config-file-structure)

---

## 1. ภาพรวม

`:config` จัดการ **settings** ของโปรแกรม ซึ่งถูกเก็บไว้ใน `math_solver.json`

```
User Input: ":config list"
  → dispatch() ตรวจ starts_with(":config ") → true
  → cmd_config("list", g_config)
```

---

## `:config list`

```cpp
// command.hpp — cmd_config()
if (sub == "list") {
    cout << "  Settings\n";
    for (const auto& key : Settings::all_keys()) {
        //  all_keys() = {"precision", "fraction_mode", "history_size", "auto_load_env"}
        string val = g_config.settings().get(key);
        cout << "    " << key << "    " << val << "\n";
    }
}
```

**Output ตัวอย่าง:**
```
  Settings
    precision       6
    fraction_mode   false
    history_size    1000
    auto_load_env   default
```

---

## `:config get`

```
Input: ":config get precision"
```

```cpp
if (sub == "get") {
    const string& key = parts[1];              // "precision"

    if (!Settings::is_valid_key(key)) {        // ตรวจว่า key ถูกต้อง
        cout << "Error: unknown setting '" << key << "'\n";
        maybe_suggest_setting(key);             // ◀── suggest ถ้าพิมพ์ผิด
        return;
    }

    cout << "  " << key << " = " << g_config.settings().get(key) << "\n";
    // Output: "  precision = 6"
}
```

### `Settings::get()` ทำอะไร

```cpp
// config.hpp
string get(const string& key) const {
    if (key == "precision")     return to_string(precision);      // "6"
    if (key == "fraction_mode") return fraction_mode ? "true" : "false";
    if (key == "history_size")  return to_string(history_size);   // "1000"
    if (key == "auto_load_env") return auto_load_env;             // "default"
    return "";
}
```

### ถ้าพิมพ์ key ผิด

```
Input: ":config get precison"    (typo: ขาด 'i')
```

- `is_valid_key("precison")` → false
- `maybe_suggest_setting("precison")`:
  - `suggest("precison", ["precision","fraction_mode","history_size","auto_load_env"])`
  - edit_distance("precison", "precision") = 1 → best!
- Output:
  ```
    Error: unknown setting 'precison'
    Did you mean precision?
  ```

---

## `:config set`

```
Input: ":config set precision 3"
```

```cpp
if (sub == "set") {
    const string& key = parts[1];       // "precision"
    // ประกอบ value จาก parts[2] เป็นต้นไป
    string value = "3";

    string err = g_config.settings().set(key, value);
    // ◀── เปลี่ยนค่า + validate

    if (!err.empty()) {
        cout << "Error: " << err << "\n";
        return;
    }

    g_config.save();                     // ◀── บันทึกลง JSON file ทันที
    cout << "  " << key << " = " << g_config.settings().get(key) << "\n";
    // Output: "  precision = 3"
}
```

### `Settings::set()` — Validation ข้างใน

```cpp
// config.hpp
string set(const string& key, const string& value) {
    if (key == "precision") {
        int v = stoi(value);              // "3" → 3
        if (v < 0 || v > 15)
            return "precision must be 0-15";
        precision = v;                     // precision = 3 ✓
    }
    else if (key == "fraction_mode") {
        if (value == "true" || value == "1" || value == "on")
            fraction_mode = true;
        else if (value == "false" || value == "0" || value == "off")
            fraction_mode = false;
        else
            return "fraction_mode must be true/false";
    }
    else if (key == "history_size") {
        int v = stoi(value);
        if (v < 1) return "history_size must be > 0";
        history_size = v;
    }
    else if (key == "auto_load_env") {
        auto_load_env = value;             // เก็บตรงๆ
    }
    return "";  // success - return empty string
}
```

### ตัวอย่าง validation error:

```
Input: ":config set precision 20"
  → set("precision", "20") → 20 > 15 → return "precision must be 0-15"
  → Output: "  Error: precision must be 0-15"

Input: ":config set fraction_mode maybe"
  → "maybe" ≠ true/false/1/0/on/off → return "fraction_mode must be true/false"

Input: ":config set history_size -5"
  → -5 < 1 → return "history_size must be > 0"
```

### Special: `auto_load_env` validation

```cpp
// ตรวจเพิ่มว่า environment ที่ระบุมีอยู่จริงหรือไม่
if (key == "auto_load_env" && !g_config.env_exists(value)) {
    cout << "  Warning: environment '" << value << "' does not exist yet\n";
    // ⚠️ Warning เฉยๆ ไม่ block — ยัง set ได้ (ผู้ใช้อาจจะสร้าง env ทีหลัง)
}
```

---

## `:config path`

```cpp
if (sub == "path") {
    cout << "  " << g_config.file_path() << "\n";
}
```

**Output ตัวอย่าง (Windows):**
```
  C:\Users\username\AppData\Roaming\math-solver\math_solver.json
```

### Config path resolution

```cpp
// config.hpp — resolve_config_path()
static string resolve_config_path() {
    // 1) ดูใน current directory ก่อน
    fs::path local_path = fs::current_path() / "math_solver.json";
    if (fs::exists(local_path))
        return local_path.string();

    // 2) ดูใน platform-specific config dir
    //    Windows: %APPDATA%\math-solver\math_solver.json
    //    Linux:   ~/.config/math-solver/math_solver.json

    // 3) Fallback: current directory
}
```

---

## `:config reset`

```cpp
if (sub == "reset") {
    g_config.reset_settings();    // ◀── reset settings เป็น default
    g_config.save();              // ◀── save ลง file
    cout << "  Settings reset to defaults\n";
}
```

```cpp
// config.hpp
void reset_settings() { settings_ = Settings(); }
// สร้าง Settings ใหม่ด้วย default constructor:
//   precision     = 6
//   fraction_mode = false
//   history_size  = 1000
//   auto_load_env = "default"
```

> **⚠️ หมายเหตุ:** `reset` จะ reset เฉพาะ **settings** ไม่ลบ **environments**

---

## Settings ทั้งหมด

| Key             | Type   | Default   | คำอธิบาย                                          | ค่าที่รับได้           |
| --------------- | ------ | --------- | ----------------------------------------------- | ----------------- |
| `precision`     | int    | 6         | ทศนิยมในการแสดงผล                                | 0-15              |
| `fraction_mode` | bool   | false     | แสดง coefficient เป็นเศษส่วน (default --fraction) | true/false/on/off |
| `history_size`  | int    | 1000      | จำนวน history entries สูงสุด                       | > 0               |
| `auto_load_env` | string | "default" | Environment ที่ load อัตโนมัติตอนเปิดโปรแกรม          | ชื่อ env            |

---

## Config File Structure

```json
{
  "settings": {
    "precision": 6,
    "fraction_mode": false,
    "history_size": 1000,
    "auto_load_env": "default"
  },
  "environments": {
    "default": {
      "variables": {
        "pi": 3.14159265358979,
        "e": 2.71828182845905,
        "tau": 6.28318530717959
      }
    },
    "physics": {
      "variables": {
        "g": 9.81,
        "c": 299792458
      }
    }
  }
}
```

### `Config::save()` ทำอะไร

```cpp
void save() const {
    // 1. สร้าง directory ถ้ายังไม่มี
    fs::create_directories(dir);

    // 2. สร้าง JSON object
    json j;
    j["settings"]     = settings_.to_json();
    j["environments"] = envs_json;

    // 3. เขียนลง file (pretty-print indent 2)
    ofstream ofs(file_path_);
    ofs << j.dump(2) << endl;
}
```
