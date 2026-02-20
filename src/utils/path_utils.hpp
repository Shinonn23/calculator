#ifndef PATH_UTILS_H
#define PATH_UTILS_H

#include <filesystem>
#include <string>

namespace math_solver {

    // Returns the canonical path to the user's history file, creating parent
    // directories if necessary.
    //
    // - On Windows, prefers %APPDATA%\math-solver\history.txt. This avoids
    //   polluting user home directories and aligns with platform conventions.
    // - On Unix-like systems, uses $HOME/.config/math-solver/history.txt to
    //   respect XDG base directory spec. Falls back to a local file if $HOME
    //   is unset (rare, but possible in restricted environments).
    //
    // Invariant: If the function returns a path, the parent directory exists
    // (or creation was attempted). Callers must not assume the file itself
    // exists.
    //
    // Note: Directory creation is best-effort; failures are not fatal here.
    // This is consistent with rustc's approach to non-critical user state.
    //
    // Performance: Directory creation is idempotent and cheap for existing
    // directories, but may incur I/O on first run.
    inline std::string get_history_file_path() {
        namespace fs = std::filesystem;

#ifdef _WIN32
        char*  appdata = nullptr;
        size_t len     = 0;
        if (_dupenv_s(&appdata, &len, "APPDATA") == 0 && appdata) {
            fs::path dir = fs::path(appdata) / "math-solver";
            free(appdata);
            fs::create_directories(dir);
            return (dir / "history.txt").string();
        }
#else
        if (const char* home = std::getenv("HOME")) {
            fs::path dir = fs::path(home) / ".config" / "math-solver";
            fs::create_directories(dir);
            return (dir / "history.txt").string();
        }
#endif
        // Fallback: HOME/APPDATA not set. Use a local file to avoid
        // hard failure. This is consistent with rustc's fallback
        // strategies for user state.
        return ".math_solver_history";
    }

} // namespace math_solver

#endif // PATH_UTILS_H
