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
3. **Run all Tests**
   ```bash
   cmake --build build --target RUN_TESTS
   ```

4. **Run tests with CTest**
   ```bash
   cd build
   ctest --output-on-failure
   ```

5. **Run the executable:**
   ```bash
   ./build/bin/Debug/math-solver.exe -f ./main.ms
   ```

## Quick Commands

| Command | Purpose |
|---------|---------|
| `cmake -B build -S .` | Configure project |
| `cmake --build build` | Build project |
| `cmake --build build --target clean` | Clean build files |
| `./build/bin/Debug/math-solver.exe -f ./main.ms` | Run executable |

## Development

The project uses:
- **CMake** for build management
- **C++17** standard
- **Header-based organization** for core modules
