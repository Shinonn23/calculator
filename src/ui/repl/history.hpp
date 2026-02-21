#pragma once

#include "config/config.hpp"
#include "utils/path_utils.hpp"

#include <replxx.hxx>
#include <string>

namespace math_solver {

    // Sets up the in-memory history size for the REPL.
    // Note: History file loading is intentionally deferred to the main loop
    // to allow for custom registry-based persistence. This avoids
    // double-loading and ensures that history state is consistent with registry
    // expectations. Returns the resolved history file path for later use.
    inline std::string setup_history(replxx::Replxx& rx, const Config& cfg) {
        rx.set_max_history_size(cfg.settings().history_size);
        const std::string path = get_history_file_path();
        return path;
    }

    // Adds a line to the REPL history if non-empty.
    // This is a low-level utility; higher-level code may bypass this
    // in favor of direct registry-managed history updates.
    inline void add_history(replxx::Replxx& rx, const std::string& line) {
        if (!line.empty())
            rx.history_add(line);
    }

} // namespace math_solver