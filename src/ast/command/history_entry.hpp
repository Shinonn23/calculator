#pragma once

//! # Module — `src/ast/command/history_entry.hpp`
//!
//! Defines `HistoryStatus`, `HistoryEntry`, and the serialization helpers
//! `to_string` / `parse_history_status`. Used by the history subsystem to
//! record and persist the outcome of each executed command.

#include <string>

namespace math_solver {

    /// Outcome status attached to a command history entry.
    ///
    /// * `Success` — Command completed without error.
    /// * `Error`   — Command failed in a user-visible way.
    /// * `Warning` — Command completed with non-fatal issues.
    /// * `Info`    — Informational record, not an error.
    /// * `Unknown` — Unrecognized status string; used for forward compatibility.
    enum class HistoryStatus { Success, Error, Warning, Info, Unknown };

    /// Convert a `HistoryStatus` to its canonical serialization string.
    ///
    /// The mapping is stable across versions; any change must be coordinated
    /// with `parse_history_status` to maintain on-disk compatibility.
    ///
    /// # Returns
    ///
    /// One of `"success"`, `"error"`, `"warning"`, `"info"`, or `"unknown"`.
    inline std::string to_string(HistoryStatus status) {
        switch (status) {
        case HistoryStatus::Success:
            return "success";
        case HistoryStatus::Error:
            return "error";
        case HistoryStatus::Warning:
            return "warning";
        case HistoryStatus::Info:
            return "info";
        default:
            return "unknown";
        }
    }

    /// Parse a serialized status string into a `HistoryStatus` value.
    ///
    /// Unrecognized strings map to `HistoryStatus::Unknown` for forward
    /// compatibility. Input is not case-folded or stripped of whitespace.
    ///
    /// # Arguments
    ///
    /// * `str` — A sanitized status string produced by `to_string`.
    ///
    /// # Returns
    ///
    /// The corresponding `HistoryStatus`, or `Unknown` if unrecognized.
    inline HistoryStatus parse_history_status(const std::string& str) {
        if (str == "success")
            return HistoryStatus::Success;
        if (str == "error")
            return HistoryStatus::Error;
        if (str == "warning")
            return HistoryStatus::Warning;
        if (str == "info")
            return HistoryStatus::Info;
        return HistoryStatus::Unknown;
    }

    /// A single entry in the command execution history.
    ///
    /// `command` holds the raw input string; `timestamp` holds an
    /// ISO-8601-style string recorded at execution time. At most one of
    /// `is_success()`, `is_error()`, `is_warning()`, and `is_info()` is true
    /// for a given entry.
    struct HistoryEntry {
        std::string   command;
        HistoryStatus status = HistoryStatus::Success;
        std::string   timestamp;

        /// Return true if the command completed successfully.
        bool is_success() const { return status == HistoryStatus::Success; }

        /// Return true if the command resulted in an error.
        bool is_error() const { return status == HistoryStatus::Error; }

        /// Return true if the command completed with a warning.
        bool is_warning() const { return status == HistoryStatus::Warning; }

        /// Return true if this is an informational history record.
        bool is_info() const { return status == HistoryStatus::Info; }

        /// Return true if the command resulted in an error.
        ///
        /// Alias for `is_error()`; retained for compatibility with legacy callers.
        bool failed() const { return status == HistoryStatus::Error; }
    };

} // namespace math_solver
