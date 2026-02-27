# `:config` Command — Complete Execution Flow

## สารบัญ
1. [ภาพรวม (Overview)](#1-ภาพรวม)
2. [Execution Flow: REPL → Parser → Handler](#2-execution-flow-repl--parser--handler)
3. [`:config list` — แสดง settings ทั้งหมด](#config-list)
4. [`:config get <key>` — ดูค่า setting](#config-get)
5. [`:config set <key> <value>` — เปลี่ยน setting](#config-set)
6. [`:config path` — ดู path ของ config file](#config-path)
7. [`:config reset` — reset เป็น default](#config-reset)
8. [Settings ทั้งหมด](#settings-ทั้งหมด)
9. [Config file structure (JSON)](#config-file-structure)

---

## 1. ภาพรวม

`:config` จัดการ **settings** ของโปรแกรม ผ่าน AST-based command system ที่ใช้ visitor pattern

```
User Input: ":config list"
  → REPL captures input (runner.cpp:57)
  → parse_command() → CommandParser::parse() (command_parser.cpp:14)
  → ConfigCommandParser builds AST (config_command_parser.cpp:6)
  → HandlerRegistry::dispatch() via visitor pattern (registry.cpp:91)
  → handle_config() executes List action (config_handler.hpp:22)
```

---

## 2. Execution Flow: REPL → Parser → Handler

### 2.1 REPL Input & Command Parsing
```cpp
// runner.cpp:27 - Main entry point
bool Runner::run_line(const std::string& line) {
    auto parse_result = parse_command(line);  // ← 1. Parse command
    if (!parse_result) {
        registry_.sink().push(parse_result.error());
        return true;
    }
    
    auto cmd = std::move(*parse_result);
    bool status = registry_.dispatch(*cmd);   // ← 2. Dispatch via visitor
    registry_.sink().flush(std::cout);
    return status;
}
```

### 2.2 Command Tokenization & AST Construction
```cpp
// command_parser.cpp:14 - Parser entry point
Result<CommandPtr> CommandParser::parse() {
    CommandTokenStream stream(raw_input_);
    
    // Registry lookup for ":config" → ConfigCommandParser
    std::string cmd = stream.peek().value;
    static SubparserRegistry registry = build_registry();
    auto it = registry.find(cmd);
    if (it != registry.end()) {
        return it->second->parse(stream);  // ← Dispatch to ConfigCommandParser
    }
    // ... fallback to Evaluate command
}
```

### 2.3 ConfigCommandParser Builds AST
```cpp
// config_command_parser.cpp:6
Result<CommandPtr> ConfigCommandParser::parse(ITokenStream& stream) {
    stream.advance(); // Advance past "config" token
    
    if (stream.is_eof()) {
        return make_unique<ConfigCommand>(ConfigCommand::Action::List, 
                                         stream.raw_input());
    }
    
    std::string sub = stream.peek().value;  // "list", "get", "set", etc.
    stream.advance();
    
    ConfigCommand::Action action = ConfigCommand::Action::Unknown;
    // Parse subcommand and determine action...
    
    auto config_cmd = std::make_unique<ConfigCommand>(action, stream.raw_input());
    // Set key/value for Get/Set actions...
    return config_cmd;
}
```

### 2.4 Visitor Pattern Dispatch
```cpp
// registry.cpp:91 - Double dispatch via visitor
void HandlerRegistry::visit(ConfigCommand& cmd, Context& ctx, 
                           Config& cfg, std::string& current_env, 
                           DiagnosticSink& sink) {
    last_command_status_ = config_reg_.dispatch(cmd.action(), cmd, ctx_, 
                                              cfg_, current_env_, sink);
}

// config_handler.hpp:13 - Handler entry point
inline HistoryStatus handle_config(const ConfigCommand& cmd, Config& config, 
                                  DiagnosticSink& sink) {
    switch (cmd.action()) {  // ← Dispatch on action enum
        case ConfigCommand::Action::List: { /* ... */ }
        case ConfigCommand::Action::Get: { /* ... */ }
        case ConfigCommand::Action::Set: { /* ... */ }
        // ...
    }
}
```

---

## `:config list`

```cpp
// config_handler.hpp:22 - List action handler
case ConfigCommand::Action::List: {
    std::ostringstream oss;
    oss << "  " << ansi::bold << "Settings" << ansi::reset << "\n";
    for (const auto& key : Settings::all_keys()) {  // ← Get all valid keys
        std::string val = config.settings().get(key);  // ← Get current value
        oss << "  " << ansi::dim << key;
        for (size_t i = key.size(); i < 28; ++i)  // ← Align formatting
            oss << ' ';
        oss << ansi::reset << val << "\n";
    }
    sink.push_output(oss.str());  // ← Send to diagnostic sink
    return HistoryStatus::Info;
}
```

**Output ตัวอย่าง:**
```
  Settings
    output.mode           auto
    output.decimals       6
    output.fraction       false
    output.trailing_zeros false
    output.thousands_sep  false
    solver.tolerance      1e-12
    solver.max_iter       1000
    history.size          1000
    history.dedup         true
    history.ignore        
    repl.prompt           > 
    repl.show_timing      false
    repl.auto_save_env    true
    repl.confirm_delete   true
    auto_load_env         
```

---

## `:config get`

```
Input: ":config get output.decimals"
```

```cpp
// config_handler.hpp:36 - Get action handler
case ConfigCommand::Action::Get: {
    const std::string& key = cmd.key();  // ← Extract key from AST
    if (key.empty()) {
        sink.push(errors::missing_setting_key(
            raw, "get", "`config get <key>`", file, line));
        return HistoryStatus::Error;
    }
    if (!Settings::is_valid_key(key)) {  // ← Validate key exists
        sink.push(errors::unknown_setting(raw, key, file, line));
        return HistoryStatus::Error;
    }
    std::ostringstream oss;
    oss << "  " << key << " = " << config.settings().get(key) << "\n";
    sink.push_output(oss.str());
    return HistoryStatus::Success;
}
```

### `Settings::get()` ทำอะไร

```cpp
// config.hpp
string get(const string& key) const {
    if (key == "output.mode")           return output_mode;           // "auto"
    if (key == "output.decimals")       return to_string(output_decimals);       // "6"
    if (key == "output.fraction")       return output_fraction ? "true" : "false";
    if (key == "output.trailing_zeros") return output_trailing_zeros ? "true" : "false";
    if (key == "output.thousands_sep")  return output_thousands_sep ? "true" : "false";
    if (key == "solver.tolerance")      return to_string(solver_tolerance);      // "1e-12"
    if (key == "solver.max_iter")       return to_string(solver_max_iter);       // "1000"
    if (key == "history.size")          return to_string(history_size);          // "1000"
    if (key == "history.dedup")         return history_dedup ? "true" : "false";
    if (key == "history.ignore")        return history_ignore;                  // ""
    if (key == "repl.prompt")           return repl_prompt;                     // "> "
    if (key == "repl.show_timing")      return repl_show_timing ? "true" : "false";
    if (key == "repl.auto_save_env")    return repl_auto_save_env ? "true" : "false";
    if (key == "repl.confirm_delete")   return repl_confirm_delete ? "true" : "false";
    if (key == "auto_load_env")         return auto_load_env;                   // ""
    return "";
}
```

### ถ้าพิมพ์ key ผิด

```
Input: ":config get output.decimal"    (typo: ขาด 's')
```

- `is_valid_key("output.decimal")` → false
- `maybe_suggest_setting("output.decimal")`:
  - `suggest("output.decimal", ["output.mode","output.decimals","output.fraction",...])`
  - edit_distance("output.decimal", "output.decimals") = 1 → best!
- Output:
  ```
    Error: unknown setting 'output.decimal'
    Did you mean output.decimals?
  ```

---

## `:config set`

```
Input: ":config set output.decimals 3"
```

```cpp
// config_handler.hpp:54 - Set action handler
case ConfigCommand::Action::Set: {
    const std::string& key   = cmd.key();    // ← Extract key from AST
    const std::string& value = cmd.value();  // ← Extract value from AST

    if (key.empty() || value.empty()) {
        sink.push(errors::missing_key_or_value(raw, file, line));
        return HistoryStatus::Error;
    }
    if (!Settings::is_valid_key(key)) {  // ← Validate key exists
        sink.push(errors::unknown_setting(raw, key, file, line));
        return HistoryStatus::Error;
    }

    // Special validation for auto_load_env
    if (key == "auto_load_env" && !value.empty() && 
        !config.env_exists(value)) {
        sink.push(errors::env_ref_error(raw, value, config, file, line));
        return HistoryStatus::Error;
    }

    std::string err_msg = config.settings().set(key, value);  // ← Set + validate
    if (!err_msg.empty()) {
        sink.push(errors::invalid_setting_value(
            raw, key, value, err_msg, file, line));
        return HistoryStatus::Error;
    }

    config.save();  // ← Persist to disk
    std::ostringstream oss;
    oss << "  " << key << " = " << config.settings().get(key) << "\n";
    sink.push_output(oss.str());
    return HistoryStatus::Success;
}
```

### `Settings::set()` — Validation ข้างใน

```cpp
// config.hpp
string set(const string& key, const string& value) {
    if (key == "output.mode") {
        if (value != "auto" && value != "decimal" && value != "exact")
            return "must be 'auto', 'decimal', or 'exact'";
        output_mode = value;
    }
    else if (key == "output.decimals") {
        int v = stoi(value);              // "3" → 3
        if (v < 0 || v > 15)
            return "must be 0-15";
        output_decimals = v;               // output_decimals = 3 ✓
    }
    else if (key == "output.fraction") {
        if (value == "true" || value == "1" || value == "on")
            output_fraction = true;
        else if (value == "false" || value == "0" || value == "off")
            output_fraction = false;
        else
            return "must be true/false";
    }
    else if (key == "solver.tolerance") {
        double v = stod(value);
        if (v <= 0)
            return "must be > 0";
        solver_tolerance = v;
    }
    else if (key == "solver.max_iter") {
        int v = stoi(value);
        if (v < 1) return "must be > 0";
        solver_max_iter = v;
    }
    else if (key == "history.size") {
        int v = stoi(value);
        if (v < 1) return "must be > 0";
        history_size = v;
    }
    // ... more validations for other keys
    return "";  // success - return empty string
}
```

### ตัวอย่าง validation error:

```
Input: ":config set output.decimals 20"
  → set("output.decimals", "20") → 20 > 15 → return "must be 0-15"
  → Output: "  Error: must be 0-15"

Input: ":config set output.fraction maybe"
  → "maybe" ≠ true/false/1/0/on/off → return "must be true/false"

Input: ":config set history.size -5"
  → -5 < 1 → return "must be > 0"

Input: ":config set solver.tolerance 0"
  → 0 ≤ 0 → return "must be > 0"
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
// config_handler.hpp:89 - Path action handler
case ConfigCommand::Action::Path: {
    std::ostringstream oss;
    oss << "  " << config.file_path() << "\n";  // ← Get resolved path
    sink.push_output(oss.str());
    return HistoryStatus::Info;
}
```

**Output ตัวอย่าง (Linux):**
```
  /home/user/.config/math-solver/math_solver.json
```

**Output ตัวอย่าง (Windows):**
```
  C:\Users\username\AppData\Roaming\math-solver\math_solver.json
```

### Config path resolution

```cpp
// config.cpp:338 - Platform-specific path resolution
std::string Config::resolve_config_path() {
    // 1) ดูใน current directory ก่อน
    fs::path local_path = fs::current_path() / "math_solver.json";
    if (fs::exists(local_path))
        return local_path.string();

    // 2) ดูใน platform-specific config dir
    //    Windows: %APPDATA%\math-solver\math_solver.json
    //    Linux:   ~/.config/math-solver/math_solver.json
    //    macOS:   ~/Library/Application Support/math-solver/math_solver.json

    // 3) Fallback: current directory
}
```

---

## `:config reset`

```cpp
// config_handler.hpp:96 - Reset action handler
case ConfigCommand::Action::Reset:
    config.reset_settings();  // ← Reset to default values
    config.save();            // ← Persist to disk
    sink.push_output("  Settings reset to defaults\n");
    return HistoryStatus::Success;
```

```cpp
// config.cpp - Settings reset implementation
void Config::reset_settings() { 
    settings_ = Settings();  // ← Create new Settings with defaults
}
// Default constructor creates:
//   output.mode = "auto"
//   output.decimals = 6
//   output.fraction = false
//   output.trailing_zeros = false
//   output.thousands_sep = false
//   solver.tolerance = 1e-12
//   solver.max_iter = 1000
//   history.size = 1000
//   history.dedup = true
//   history.ignore = ""
//   repl.prompt = "> "
//   repl.show_timing = false
//   repl.auto_save_env = true
//   repl.confirm_delete = true
//   auto_load_env = ""
```

> **⚠️ หมายเหตุ:** `reset` จะ reset เฉพาะ **settings** ไม่ลบ **environments**

---

## Settings ทั้งหมด

### Output Settings
| Key                     | Type   | Default | คำอธิบาย                                | ค่าที่รับได้              |
| ----------------------- | ------ | ------- | ------------------------------------- | -------------------- |
| `output.mode`           | string | "auto"  | โหมดการแสดงผลเลข (auto/decimal/exact) | auto, decimal, exact |
| `output.decimals`       | int    | 6       | จำนวนทศนิยมในการแสดงผล                  | 0-15                 |
| `output.fraction`       | bool   | false   | แสดงเลขเป็นเศษส่วนเมื่อเป็นไปได้            | true/false/on/off    |
| `output.trailing_zeros` | bool   | false   | แสดงเลขศูนย์ต่อท้ายในทศนิยม                | true/false/on/off    |
| `output.thousands_sep`  | bool   | false   | แสดงเครื่องหมายคั่นพันคอมม่า (1,000)        | true/false/on/off    |

### Solver Settings
| Key                | Type   | Default | คำอธิบาย                         | ค่าที่รับได้     |
| ------------------ | ------ | ------- | ------------------------------ | ----------- |
| `solver.tolerance` | double | 1e-12   | ค่าความคลาดเคลื่อนที่ยอมรับในการคำนวณ | > 0         |
| `solver.max_iter`  | int    | 1000    | จำนวนรอบสูงสุดในการคำนวณแบบวนซ้ำ     | 1-1,000,000 |

### History Settings
| Key              | Type   | Default | คำอธิบาย                              | ค่าที่รับได้           |
| ---------------- | ------ | ------- | ----------------------------------- | ----------------- |
| `history.size`   | int    | 1000    | จำนวน history entries สูงสุด           | > 0               |
| `history.dedup`  | bool   | true    | เปิดการลบรายการซ้ำใน history           | true/false/on/off |
| `history.ignore` | string | ""      | รูปแบบคำสั่งที่จะไม่บันทึกใน history (regex) | สตริงว่าง/regex     |

### REPL Settings
| Key                   | Type   | Default | คำอธิบาย                   | ค่าที่รับได้           |
| --------------------- | ------ | ------- | ------------------------ | ----------------- |
| `repl.prompt`         | string | "> "    | พรอมต์ของ REPL            | สตริงใดๆ           |
| `repl.show_timing`    | bool   | false   | แสดงเวลาในการประมวลผลคำสั่ง | true/false/on/off |
| `repl.auto_save_env`  | bool   | true    | บันทึก environment อัตโนมัติ  | true/false/on/off |
| `repl.confirm_delete` | bool   | true    | ยืนยันก่อนลบ environment    | true/false/on/off |

### General Settings
| Key             | Type   | Default | คำอธิบาย                                 | ค่าที่รับได้    |
| --------------- | ------ | ------- | -------------------------------------- | ---------- |
| `auto_load_env` | string | ""      | Environment ที่ load อัตโนมัติตอนเปิดโปรแกรม | ชื่อ env/ว่าง |

---

## Config File Structure

```json
{
  "settings": {
    "output": {
      "mode": "auto",
      "decimals": 6,
      "fraction": false,
      "trailing_zeros": false,
      "thousands_sep": false
    },
    "solver": {
      "tolerance": 1e-12,
      "max_iter": 1000
    },
    "history": {
      "size": 1000,
      "dedup": true,
      "ignore": ""
    },
    "repl": {
      "prompt": "> ",
      "show_timing": false,
      "auto_save_env": true,
      "confirm_delete": true
    },
    "auto_load_env": ""
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
// config.cpp:414 - Config persistence implementation
void Config::save() const {
    // 1. สร้าง directory ถ้ายังไม่มี
    fs::create_directories(file_path_.parent_path());

    // 2. สร้าง JSON object
    json j;
    j["settings"]     = settings_.to_json();      // ← Serialize settings
    j["environments"] = json::object();           // ← Serialize environments
    for (const auto& [name, env] : envs_) {
        j["environments"][name] = env.to_json();
    }

    // 3. เขียนลง file (pretty-print indent 2)
    if (std::ofstream ofs(file_path_); ofs)
        ofs << j.dump(2) << '\n';
}
```

### `Settings::to_json()` Serialization

```cpp
// config.cpp:105 - Settings JSON serialization
json Settings::to_json() const {
    json j;
    j["output"]["mode"]           = output_mode;
    j["output"]["decimals"]       = output_decimals;
    j["output"]["fraction"]       = output_fraction;
    j["output"]["trailing_zeros"] = output_trailing_zeros;
    j["output"]["thousands_sep"]  = output_thousands_sep;
    j["solver"]["tolerance"]      = solver_tolerance;
    j["solver"]["max_iter"]       = solver_max_iter;
    j["history"]["size"]          = history_size;
    j["history"]["dedup"]         = history_dedup;
    j["history"]["ignore"]        = history_ignore;
    j["repl"]["prompt"]           = repl_prompt;
    j["repl"]["show_timing"]      = repl_show_timing;
    j["repl"]["auto_save_env"]    = repl_auto_save_env;
    j["repl"]["confirm_delete"]   = repl_confirm_delete;
    j["auto_load_env"]            = auto_load_env;
    return j;
}
