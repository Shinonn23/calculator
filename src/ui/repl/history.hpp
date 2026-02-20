#ifndef REPL_HISTORY_HPP
#define REPL_HISTORY_HPP

#include "config/config.hpp"
#include "utils/path_utils.hpp"

#include <replxx.hxx>
#include <string>

namespace math_solver {

    // History management is intentionally decoupled from REPL logic to allow
    // for flexible persistence strategies and to avoid entangling user state
    // with session logic. All functions here assume that the caller ensures
    // thread safety and lifetime of the Replxx instance.

    inline std::string setup_history(replxx::Replxx& rx, const Config& cfg) {
        // Sets up history with the configured maximum size and loads persisted
        // entries from disk. The resolved path is returned to ensure that
        // subsequent save operations target the same file, avoiding accidental
        // history fragmentation. Assumes get_history_file_path() is stable
        // across the session.
        rx.set_max_history_size(cfg.settings().history_size);
        const std::string path = get_history_file_path();
        rx.history_load(path);
        return path;
    }

    inline void save_history(replxx::Replxx& rx, const std::string& path) {
        // Persists in-memory history to disk. Callers must guarantee that
        // 'path' matches the one used in setup_history to avoid data loss.
        // No atomicity guarantees; interrupted writes may corrupt history.
        rx.history_save(path);
    }

    inline void add_history(replxx::Replxx& rx, const std::string& line) {
        // Only non-empty lines are added to history to avoid polluting the
        // persistent log with spurious entries. Assumes caller has already
        // performed any necessary normalization or deduplication.
        if (!line.empty())
            rx.history_add(line);
    }

} // namespace math_solver

#endif // REPL_HISTORY_HPP
