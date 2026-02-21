#pragma once
#include <string>

namespace math_solver {

    // Status for command execution history.
    // - Success: Command completed without error.
    // - Error: Command failed in a way that should be surfaced to the user.
    // - Warning: Command completed with non-fatal issues.
    // - Info: Informational entry, not an error or warning.
    // - Unknown: Used for forward compatibility with unrecognized status
    // strings.
    enum class HistoryStatus { Success, Error, Warning, Info, Unknown };

    // Converts HistoryStatus to a stable string representation for
    // serialization. Invariant: The mapping must remain stable across versions
    // for on-disk compatibility. Any change here must be coordinated with
    // parse_history_status.
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

    // Parses a string into HistoryStatus.
    // Returns Unknown for unrecognized input to allow forward compatibility.
    // Assumes input is sanitized; does not handle case-insensitivity or
    // whitespace.
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

    struct HistoryEntry {
        std::string   command;
        HistoryStatus status = HistoryStatus::Success;
        std::string   timestamp;

        // These predicates are used throughout the codebase to check command
        // outcomes. is_success() and failed() are both provided for
        // compatibility with legacy code. Invariant: Only one of is_success(),
        // is_error(), is_warning(), is_info() is true at a time.
        bool is_success() const { return status == HistoryStatus::Success; }
        bool is_error() const { return status == HistoryStatus::Error; }
        bool is_warning() const { return status == HistoryStatus::Warning; }
        bool is_info() const { return status == HistoryStatus::Info; }

        // Retained for compatibility with older code expecting failed() as an
        // error check.
        bool failed() const { return status == HistoryStatus::Error; }
    };

} // namespace math_solver