# `:env` Command — Walkthrough แบบละเอียด

## สารบัญ
1. [ภาพรวม (Overview)](#1-ภาพรวม)
2. [`:env` (ไม่มี args) — แสดง environment ปัจจุบัน](#env-no-args)
3. [`:env list` — แสดงรายชื่อ env ทั้งหมด](#env-list)
4. [`:env load <name>` — สลับ environment](#env-load)
5. [`:env save [name] [vars]` — บันทึกตัวแปร](#env-save)
6. [`:env new <name>` — สร้าง environment ใหม่](#env-new)
7. [`:env delete <name>` — ลบ environment](#env-delete)
8. [ความสัมพันธ์ระหว่าง Context, Environment, Config](#ความสัมพันธ์)

---

## 1. ภาพรวม

**Environment** คือ **ชุดตัวแปรที่ save ไว้** ในไฟล์ `math_solver.json` ผู้ใช้สามารถสลับไปมาระหว่างหลายๆ environment ได้ ซึ่งแต่ละ environment เก็บตัวแปรต่างกัน

```
ความสัมพันธ์:

  math_solver.json (on disk)          RAM (runtime)
  ┌───────────────────────┐           ┌──────────────┐
  │ environments:         │           │ g_ctx:       │
  │   "default":          │  ──load→  │   {pi: 3.14, │
  │     {pi:3.14, e:2.71} │           │    e: 2.71}  │
  │   "physics":          │           └──────────────┘
  │     {g:9.81, c:3e8}   │           g_current_env = "default"
  └───────────────────────┘
```

```
User Input: ":env"
  → dispatch() ตรวจ input == ":env" → true
  → cmd_env("", g_current_env, g_config, g_ctx)
```

---

## `:env` (ไม่มี args)

```cpp
// command.hpp — cmd_env()
if (parts.empty()) {
    cout << "  Current environment: " << g_current_env
         << "  (" << g_ctx.size() << " variables)\n";
}
```

**ตัวอย่าง:**
```
  Current environment: default  (3 variables)
```

---

## `:env list`

```cpp
if (sub == "list") {
    auto envs = g_config.list_envs();       // ดึงรายชื่อ env ทั้งหมดจาก config
    // list_envs() return vector<string> sorted alphabetically

    cout << "  Environments\n";
    for (const auto& name : envs) {
        if (name == g_current_env) {
            cout << "    * " << name << "  (active)\n";    // สีเขียว + ดาว
        } else {
            cout << "      " << name << "\n";
        }
    }
}
```

**สมมติ:** config มี environments: `default`, `physics`, `homework`

**Output:**
```
  Environments
    * default  (active)
      homework
      physics
```

---

## `:env load <name>`

**Input:** `:env load physics`

### Flow ทั้งหมด

```cpp
if (sub == "load") {
    const string& name = parts[1];           // name = "physics"

    if (name == g_current_env) {             // ถ้า load env เดิม → ไม่ทำอะไร
        cout << "  Already in environment '" << name << "'\n";
        return;
    }

    load_env_into_context(name, g_ctx, g_config, g_current_env);
    // ◀── ฟังก์ชันจาก environment.hpp
}
```

### ข้างใน `load_env_into_context()`

```cpp
// environment.hpp
void load_env_into_context(const string& env_name, Context& g_ctx,
                            Config& g_config, string& g_current_env) {

    // 1. ตรวจว่า env_name มีอยู่จริง
    if (!g_config.env_exists(env_name)) {
        cout << "Error: environment '" << env_name << "' not found\n";
        // suggest ชื่อที่ใกล้เคียง
        auto match = suggest(env_name, g_config.list_envs());
        if (match) cout << "  Did you mean " << *match << "?\n";
        return;
    }

    // 2. ⚡ Save env ปัจจุบันก่อน! (กันข้อมูลหาย)
    save_current_env_to_config(g_config, g_current_env, g_ctx);
    //   → g_config.save_env_variables("default", g_ctx.all())
    //   → copy ตัวแปรทั้งหมดจาก g_ctx ไปเก็บใน config["default"]

    // 3. Clear context แล้ว load env ใหม่
    g_ctx.clear();
    const auto& env = g_config.get_env(env_name);     // ดึง Environment struct
    for (const auto& [name, value] : env.variables) {
        g_ctx.set(name, value);                        // ใส่ทีละตัว
    }

    // 4. Update current env name
    g_current_env = env_name;                           // "physics"
}
```

### ตัวอย่างทีละขั้นตอน

**สมมติ:**
- `g_current_env = "default"`, `g_ctx = {pi:3.14, e:2.71, x:5}`
- Config มี `physics = {g:9.81, c:299792458}`

| Step         | สิ่งที่เกิดขึ้น                       | `g_ctx`                  | Config["default"]          |
| ------------ | ------------------------------ | ------------------------ | -------------------------- |
| ก่อน          | —                              | `{pi:3.14, e:2.71, x:5}` | `{pi:3.14, e:2.71}`        |
| save current | copy g_ctx → config["default"] | ไม่เปลี่ยน                  | `{pi:3.14, e:2.71, x:5}` ✨ |
| clear + load | clear → load physics           | `{g:9.81, c:299792458}`  | ไม่เปลี่ยน                    |
| update name  | `g_current_env = "physics"`    | —                        | —                          |

> **สังเกต:** `x:5` ที่เพิ่มมาระหว่าง session ถูก **save กลับ** ไปใน config["default"] ก่อนที่จะสลับ environment — ดังนั้นข้อมูลไม่หาย!

**Output:**
```
  Switched to physics  (2 variables)
```

---

## `:env save`

มี 3 รูปแบบ:

| รูปแบบ                  | ตัวอย่าง                  | ผลลัพธ์                        |
| ---------------------- | ----------------------- | ---------------------------- |
| **save ทั้งหมด**         | `:env save`             | save g_ctx ทั้งหมดลง env ปัจจุบัน |
| **save ไป env อื่น**     | `:env save physics`     | save g_ctx ทั้งหมดลง `physics` |
| **save เฉพาะบาง vars** | `:env save physics x y` | save เฉพาะ x, y ลง `physics` |

### Flow: `:env save` (ไม่มี args)

```cpp
if (sub == "save") {
    string name = (parts.size() >= 2) ? parts[1] : g_current_env;
    // ไม่มี args → name = g_current_env (เช่น "default")

    // ...ไม่มี parts[2] → save ทั้งหมด
    g_config.save_env_variables(name, g_ctx.all());
    g_config.save();
    cout << "  Saved " << g_ctx.size() << " variable(s) to '" << name << "'\n";
}
```

### Flow: `:env save homework x y z`

```cpp
string name = parts[1];                // "homework"

// สร้าง env ถ้ายังไม่มี
if (!g_config.env_exists(name)) {
    g_config.create_env(name);
}

// parts.size() >= 3 → save เฉพาะ vars ที่ระบุ
unordered_map<string, double> selected;
vector<string>                not_found;

for (size_t i = 2; i < parts.size(); ++i) {
    const string& var = parts[i];          // "x", "y", "z"
    if (g_ctx.has(var)) {
        selected[var] = g_ctx.all().at(var);
    } else {
        not_found.push_back(var);
    }
}
```

**ตัวอย่าง:** `g_ctx = {x:5, y:3, pi:3.14}`

| var | `g_ctx.has()` | ไป selected?        |
| --- | ------------- | ------------------- |
| `x` | ✅             | `selected["x"] = 5` |
| `y` | ✅             | `selected["y"] = 3` |
| `z` | ❌             | `not_found = ["z"]` |

```
Warning: variable 'z' not found, skipped
  Did you mean x?    ← suggest ถ้ามี candidate ใกล้เคียง
```

จากนั้น **merge** เข้ากับ env ที่มีอยู่:

```cpp
auto& env = g_config.get_env(name);
for (const auto& [k, v] : selected) {
    env.variables[k] = v;              // update/insert ทีละตัว
}
g_config.save();
```

> **⚡ สำคัญ:** merge ไม่ใช่ replace — ถ้า `homework` มี `a:10` อยู่แล้ว มันจะยังคงอยู่ แค่เพิ่ม `x:5, y:3` เข้าไป

---

## `:env new <name>`

**Input:** `:env new lab3`

```cpp
if (sub == "new") {
    const string& name = parts[1];                    // "lab3"

    if (g_config.env_exists(name)) {
        cout << "Error: environment '" << name << "' already exists\n";
        return;
    }

    // 1. Save env ปัจจุบันก่อน
    save_current_env_to_config(g_config, g_current_env, g_ctx);

    // 2. สร้าง env เปล่า
    g_config.create_env(name);
    //   → envs_["lab3"] = Environment{name:"lab3", variables:{}}

    // 3. Clear context (เริ่มต้นด้วย 0 ตัวแปร)
    g_ctx.clear();

    // 4. สลับไป env ใหม่
    g_current_env = name;                              // "lab3"

    // 5. Save config file
    g_config.save();

    cout << "  Created and switched to lab3\n";
}
```

### ตัวอย่างทีละขั้นตอน

**สมมติ:** `g_current_env = "default"`, `g_ctx = {pi:3.14, x:5}`

| Step         | `g_ctx`          | `g_current_env` | Config                     |
| ------------ | ---------------- | --------------- | -------------------------- |
| save current | `{pi:3.14, x:5}` | `"default"`     | default ← `{pi:3.14, x:5}` |
| create env   | —                | —               | + `lab3: {}`               |
| clear        | `{}`             | —               | —                          |
| switch       | `{}`             | `"lab3"`        | —                          |

> **หมายเหตุ:** env ใหม่จะ **เปล่า** ไม่มีตัวแปรเลย — ไม่มี `pi`, `e` แบบ default

---

## `:env delete <name>`

**Input:** `:env delete physics`

```cpp
if (sub == "delete") {
    const string& name = parts[1];

    // ห้ามลบ env ที่ active อยู่
    if (name == g_current_env) {
        cout << "Error: cannot delete the active environment\n";
        cout << "  Switch to another environment first with :env load\n";
        return;
    }

    // ตรวจว่า env มีอยู่
    if (!g_config.env_exists(name)) {
        cout << "Error: environment '" << name << "' not found\n";
        auto match = suggest(name, g_config.list_envs());
        if (match) cout << "  Did you mean " << *match << "?\n";
        return;
    }

    // ลบจาก config
    g_config.delete_env(name);
    //   → envs_.erase("physics")
    g_config.save();

    cout << "  Deleted environment '" << name << "'\n";
}
```

### ข้างใน `Config::delete_env()`

```cpp
void delete_env(const string& name) {
    auto it = envs_.find(name);
    if (it == envs_.end())
        throw runtime_error("environment '" + name + "' not found");
    envs_.erase(it);     // ลบจาก map
}
```

---

## ความสัมพันธ์

```
┌─────────────────────────────────────────────────────────┐
│                    RAM (Runtime)                         │
│                                                         │
│  g_ctx (Context)          g_current_env (string)        │
│  ┌─────────────────┐      "default"                     │
│  │ pi = 3.14159    │                                    │
│  │ e  = 2.71828    │                                    │
│  │ x  = 5          │ ←── ตัวแปรที่ user เพิ่มระหว่าง    │
│  └─────────────────┘      session                       │
│         ↕ load/save                                     │
├─────────────────────────────────────────────────────────┤
│                   Config (g_config)                      │
│                                                         │
│  settings_: {precision:6, fraction_mode:false, ...}     │
│                                                         │
│  envs_:                                                 │
│  ┌──────────┬──────────────────────────────┐            │
│  │ "default"│ {pi:3.14, e:2.71, tau:6.28}  │            │
│  │ "physics"│ {g:9.81, c:299792458}        │            │
│  │ "lab3"   │ {}                           │            │
│  └──────────┴──────────────────────────────┘            │
│         ↕ save()/load()                                 │
├─────────────────────────────────────────────────────────┤
│              math_solver.json (Disk)                    │
│  JSON file ที่เก็บ settings + environments ทั้งหมด       │
└─────────────────────────────────────────────────────────┘
```

### Lifecycle ของ environment

```
เปิดโปรแกรม → load config → load auto_load_env → g_ctx
             ↓
ใช้งาน (set x 5, solve, etc.) → g_ctx เปลี่ยน
             ↓
:env load → save_current_env → clear → load_new_env
             ↓
ปิดโปรแกรม → save_current_env → save config → เขียนลง JSON
```

### เมื่อเปิดโปรแกรม (main.cpp)

```cpp
// main.cpp
g_config.load();                            // อ่าน JSON file

const string& auto_env = g_config.settings().auto_load_env;
// auto_env = "default" (ค่าจาก settings)

if (g_config.env_exists(auto_env)) {
    const auto& env = g_config.get_env(auto_env);
    for (const auto& [name, value] : env.variables)
        g_ctx.set(name, value);              // copy ตัวแปรจาก env ลง g_ctx
    g_current_env = auto_env;
}
```

### เมื่อปิดโปรแกรม (main.cpp)

```cpp
// main.cpp — ก่อน return 0
save_current_env_to_config(g_config, g_current_env, g_ctx);
g_config.save();                            // เขียนทั้งหมดลง JSON
rx.history_save(hist_path);                 // save command history
```
