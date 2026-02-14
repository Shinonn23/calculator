
# Math Solver

A C++ project for solving and manipulating mathematical expressions.

---

## 📦 Requirements

- CMake 3.10+
- C++17 compatible compiler
- Git (for dependency fetching)

---

## 🚀 Build Instructions

### 1️⃣ Configure Project

```bash
cmake -B build -S .
````

---

### 2️⃣ Build Project

#### Windows (Visual Studio generator)

```bash
cmake --build build --config Release
```

#### Linux / macOS (Single-config generators)

```bash
cmake --build build
```

---

### 3️⃣ Run Executable

#### Windows

```bash
./build/bin/Release/math-solver.exe
```

#### Linux / macOS

```bash
./build/bin/math-solver
```

---

## ⚡ Quick Commands

| Command                                | Purpose                              |
| -------------------------------------- | ------------------------------------ |
| `cmake -B build -S .`                  | Configure project                    |
| `cmake --build build`                  | Build (single-config generators)     |
| `cmake --build build --config Release` | Build Release (Visual Studio / MSVC) |
| `cmake --build build --target clean`   | Clean build artifacts                |
| `./build/bin/Release/math-solver.exe`  | Run program (Windows)                |
| `./build/bin/math-solver`              | Run program (Linux/macOS)            |

---

## 🛠 Development Notes

This project uses:

* **CMake** for build management
* **C++17** standard
* **FetchContent** for third-party dependencies
* Modular core architecture

---

## 📁 Project Structure

```
math-solver/
│
├── src/
│   ├── core/
│   └── main.cpp
│
├── build/          # Generated build files
├── CMakeLists.txt
└── README.md
```

---

## 🧹 Cleaning Build

```bash
cmake --build build --target clean
```

Or remove build directory:

```bash
rm -rf build
```

---

## 📚 Dependencies

* nlohmann/json
* replxx (interactive CLI input)

Dependencies are fetched automatically during configuration.

---

## 🧪 Recommended Workflow

```bash
cmake -B build -S .
cmake --build build --config Release
./build/bin/Release/math-solver.exe
```