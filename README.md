# Math Solver

A C++ project for solving and manipulating mathematical expressions.

## Build Instructions
### Build Steps

1. **Configure with CMake:**
   ```bash
   cmake -B build -S .
   ```

2. **Build the project:**
   ```bash
   cmake --build build
   ```

3. **Run the executable:**
   ```bash
   ./build/bin/Release/math-solver.exe
   ```

## Quick Commands

| Command | Purpose |
|---------|---------|
| `cmake -B build -S .` | Configure project |
| `cmake --build build` | Build project |
| `cmake --build build --target clean` | Clean build files |
| `./build/bin/latex-solver` | Run executable |

## Development

The project uses:
- **CMake** for build management
- **C++17** standard
- **Header-based organization** for core modules
