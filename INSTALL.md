# Installation Guide

This document explains how to install the required tools and build **Math Solver** on Windows and Linux.

---

## Required Tools

| Tool        | Version       |
| ----------- | ------------- |
| CMake       | 3.16+         |
| Ninja       | Latest        |
| MSVC        | v143+         |
| GCC / Clang | C++17 support |
| Git         | Any           |

---

## VS Code Extensions

The project includes `.vscode/extensions.json` with recommended extensions. VS Code will prompt you to install them automatically when you open the workspace.

| Extension ID                            | Purpose             |
| --------------------------------------- | ------------------- |
| `llvm-vs-code-extensions.vscode-clangd` | C++ language server |
| `xaver.clang-format`                    | Code formatting     |
| `ms-vscode.cmake-tools`                 | CMake integration   |
| `ms-vscode.cpptools-extension-pack`     | C++ tooling pack    |

---

# Windows

## 1. Install Visual Studio 2022 Build Tools

Download from: https://visualstudio.microsoft.com/downloads/

Select **Build Tools for Visual Studio 2022** and install:

- **Workload:** Desktop development with C++
- **Components:** MSVC v143 C++ toolset, Windows 10/11 SDK

## 2. Install CMake

```powershell
winget install Kitware.CMake
```

Verify:

```powershell
cmake --version
```

## 3. Install Ninja

```powershell
winget install Ninja-build.Ninja
```

Verify:

```powershell
ninja --version
```

## 4. Install Git

```powershell
winget install Git.Git
```

## 5. Install clang-format (optional, for code formatting)

```powershell
winget install LLVM.LLVM
```

Verify:

```powershell
clang-format --version
```

---

# Linux

## Ubuntu / Debian

```bash
sudo apt update
sudo apt install build-essential cmake ninja-build git clang-format
```

## Arch Linux

```bash
sudo pacman -S base-devel cmake ninja git clang
```

---

# Build

## Step 1 — Open Build Environment

### Windows

Open **Developer PowerShell for Visual Studio 2022**.

Or source the environment from a regular PowerShell session:

```powershell
. .\vsdev.ps1
```

### Linux

Use any terminal.

## Step 2 — Clone Repository

```bash
git clone <repository-url>
cd math-solver
```

## Step 3 — Configure

```bash
cmake -S . -B build -G Ninja -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
```

This will:

- Generate Ninja build files in `build/`
- Generate `compile_commands.json` for clangd
- Fetch all dependencies automatically (nlohmann/json, replxx)

## Step 4 — Build

```bash
ninja -C build
```

## Step 5 — Run

### Windows

```powershell
.\build\bin\math-solver.exe
```

### Linux

```bash
./build/bin/math-solver
```

---

# Clean Build

Remove the build directory and start fresh:

```bash
rm -rf build
cmake -S . -B build -G Ninja -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
ninja -C build
```

---

# Editor Configuration

The workspace ships with `.vscode/settings.json` pre-configured for:

- **clangd** as the C++ language server (Microsoft C++ IntelliSense is disabled)
- **clang-format** for formatting with format-on-save enabled
- `compile_commands.json` read from `build/`

clangd is configured with these flags:

```
--compile-commands-dir=build
--background-index
--clang-tidy
--all-scopes-completion
--cross-file-rename
--pch-storage=memory
```

---

# Troubleshooting

### Ninja not found

Make sure Ninja is installed and on your `PATH`:

```bash
ninja --version
```

### Compiler not detected (Windows)

Open a **Developer PowerShell for Visual Studio**, or run:

```powershell
. .\vsdev.ps1
```

### clangd not finding headers

Rebuild `compile_commands.json`:

```bash
cmake -S . -B build -G Ninja -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
```

Then restart clangd in VS Code: `Ctrl+Shift+P` → **clangd: Restart language server**.
