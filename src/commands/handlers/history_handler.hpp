#pragma once

#include "ast/command/history_command.hpp"
#include "ast/command/history_entry.hpp"
#include "core/error.hpp"
#include "ui/color.hpp"
#include "utils/path_utils.hpp"
#include "utils/string_utils.hpp"

#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace math_solver {
    namespace handlers {

        // This is a placeholder for future filtering logic.
        inline bool is_history_noise(const std::string& /*cmd*/) {
            return false;
        }

        // Appends a single HistoryEntry to the persistent history file.
        // Assumes file path is valid and writable; errors are ignored.
        // Format: timestamp [status]\ncommand\n\n
        inline void append_history_file(const HistoryEntry& entry) {
            std::ofstream file(get_history_file_path(),
                               std::ios::app | std::ios::out);
            if (file.is_open()) {
                file << entry.timestamp << " [" << to_string(entry.status)
                     << "]\n";
                file << entry.command << "\n\n";
            }
        }

        // Loads all HistoryEntry records from the persistent history file.
        // Invariant: Each entry is two lines (header, command), separated by
        // blank lines. Skips malformed or partial entries; only well-formed
        // pairs are loaded. Assumes file is not concurrently mutated.
        inline std::vector<HistoryEntry> load_history_file() {
            std::vector<HistoryEntry> history;
            std::ifstream             file(get_history_file_path());
            if (!file.is_open())
                return history;

            std::string line;
            while (std::getline(file, line)) {
                std::string trimmed_line = trim(line);
                if (trimmed_line.empty())
                    continue;

                size_t bracket_start = trimmed_line.find('[');
                size_t bracket_end   = trimmed_line.rfind(']');

                bool   is_header     = (bracket_start != std::string::npos &&
                                  bracket_end != std::string::npos &&
                                  bracket_end > bracket_start);

                if (is_header) {
                    HistoryEntry entry;
                    entry.timestamp =
                        trim(trimmed_line.substr(0, bracket_start));
                    std::string status_str = trim(trimmed_line.substr(
                        bracket_start + 1, bracket_end - bracket_start - 1));
                    entry.status           = parse_history_status(status_str);

                    // Reads the next line as the command; if missing, entry is
                    // skipped.
                    if (std::getline(file, line)) {
                        entry.command = trim(line);
                        history.push_back(entry);
                    }
                }
                // Non-header lines are ignored to avoid misalignment.
            }
            return history;
        }

        // Validates that all indices in `range` are within the bounds of
        // `history`. Returns false and sets `out_error` if any index is out of
        // bounds. Indices are 1-based externally, 0-based internally.
        inline bool resolve_range(const std::vector<int>&         range,
                                  const std::vector<std::string>& history,
                                  std::vector<int>&               out_indices,
                                  std::string&                    out_error) {
            int max = static_cast<int>(history.size());
            for (int idx : range) {
                if (idx < 1 || idx > max) {
                    out_error = "index " + std::to_string(idx) +
                                " out of range (history has " +
                                std::to_string(max) + " entries)";
                    return false;
                }
                out_indices.push_back(idx - 1);
            }
            return true;
        }

        // Pretty-prints a list of history entries with colorized status.
        // Assumes indices are 1-based for user display.
        inline void print_history_entries(
            const std::vector<std::pair<int, HistoryEntry>>& entries) {
            for (const auto& [idx, entry] : entries) {
                std::cout << "  " << ansi::dim << "[" << idx << "] "
                          << entry.timestamp << ansi::reset;

                if (entry.is_success()) {
                    std::cout << ansi::green << " [ok] ";
                } else if (entry.is_error()) {
                    std::cout << ansi::red << " [error] ";
                } else if (entry.is_warning()) {
                    std::cout << ansi::yellow << " [warn] ";
                } else {
                    std::cout << ansi::dim << " [info] ";
                }

                std::cout << ansi::reset << entry.command << "\n";
            }
        }

        // Writes the provided entries to the specified file.
        // Overwrites any existing file. Returns false on I/O failure.
        // Entries are written in the same format as append_history_file.
        inline bool save_history_to_file(
            const std::string&                               filepath,
            const std::vector<std::pair<int, HistoryEntry>>& entries) {
            std::ofstream file(filepath);
            if (!file.is_open())
                return false;

            for (const auto& [idx, entry] : entries) {
                file << entry.timestamp << " [" << to_string(entry.status)
                     << "]\n"
                     << entry.command << "\n\n";
            }
            return true;
        }

        // Determines if a HistoryEntry should be shown given the command flags.
        // If no flags are set, all entries are shown.
        // Otherwise, only entries matching at least one requested status are
        // shown.
        inline bool should_show_entry(const HistoryEntry&   entry,
                                      const HistoryCommand& cmd) {
            if (!cmd.has_any_flag())
                return true;

            if (cmd.has_flag(HistoryCommand::Flag::Success) &&
                entry.is_success())
                return true;
            if (cmd.has_flag(HistoryCommand::Flag::Errors) && entry.is_error())
                return true;
            if (cmd.has_flag(HistoryCommand::Flag::Warning) &&
                entry.is_warning())
                return true;
            if (cmd.has_flag(HistoryCommand::Flag::Info) && entry.is_info())
                return true;

            return false;
        }

        // Main entry point for handling history commands.
        // Dispatches based on HistoryCommand::Action.
        // - Show: prints last N entries, filtered by flags.
        // - ShowRange: prints specific indices, with bounds checking.
        // - Search: prints entries whose command contains the pattern.
        // - Save: writes filtered entries to a file.
        // - Clear: truncates the persistent history file.
        // Returns HistoryStatus indicating result for diagnostics.
        // Assumes session_history is a consistent snapshot.
        inline HistoryStatus
        handle_history(const HistoryCommand&            cmd,
                       const std::vector<HistoryEntry>& session_history) {

            const std::string& raw = cmd.raw_command();

            switch (cmd.action()) {
            case HistoryCommand::Action::Show: {
                if (session_history.empty()) {
                    std::cout << "  No history\n";
                    return HistoryStatus::Info;
                }

                int total = static_cast<int>(session_history.size());
                int start =
                    (cmd.limit() == 0) ? 0 : std::max(0, total - cmd.limit());

                std::vector<std::pair<int, HistoryEntry>> entries;
                for (int i = start; i < total; ++i) {
                    if (should_show_entry(session_history[i], cmd)) {
                        entries.emplace_back(i + 1, session_history[i]);
                    }
                }

                print_history_entries(entries);
                return HistoryStatus::Success;
            }

            case HistoryCommand::Action::ShowRange: {
                std::vector<int>         indices;
                std::string              err;
                std::vector<std::string> commands;
                for (const auto& entry : session_history)
                    commands.push_back(entry.command);

                if (!resolve_range(cmd.range(), commands, indices, err)) {
                    size_t space_pos =
                        raw.find_first_of(" \t", raw.find(":history"));
                    size_t start_span =
                        (space_pos != std::string::npos)
                            ? raw.find_first_not_of(" \t", space_pos)
                            : raw.length();

                    MathError math_err(
                        err, Span(start_span, raw.length()), raw);
                    math_err.with_code("E0701").with_label("invalid range");
                    std::cout << math_err.format();
                    return HistoryStatus::Error;
                }

                std::vector<std::pair<int, HistoryEntry>> entries;
                for (int i : indices) {
                    if (should_show_entry(session_history[i], cmd)) {
                        entries.emplace_back(i + 1, session_history[i]);
                    }
                }

                print_history_entries(entries);
                return HistoryStatus::Success;
            }

            case HistoryCommand::Action::Search: {
                const std::string& pattern = cmd.pattern();
                if (pattern.empty()) {
                    MathError err("missing search pattern",
                                  find_token_span(raw, "search"),
                                  raw);
                    err.with_code("E0702").with_help(
                        "Usage: `:history search <pattern>`");
                    std::cout << err.format();
                    return HistoryStatus::Error;
                }

                std::vector<std::pair<int, HistoryEntry>> matches;
                for (int i = 0; i < static_cast<int>(session_history.size());
                     ++i) {
                    if (session_history[i].command.find(pattern) !=
                        std::string::npos) {
                        if (should_show_entry(session_history[i], cmd)) {
                            matches.emplace_back(i + 1, session_history[i]);
                        }
                    }
                }

                if (matches.empty()) {
                    std::cout << "  No matches found for '" << pattern
                              << "' with current filters\n";
                    return HistoryStatus::Info;
                } else {
                    print_history_entries(matches);
                    return HistoryStatus::Success;
                }
            }

            case HistoryCommand::Action::Save: {
                if (cmd.filepath().empty()) {
                    MathError err(
                        "missing file path", find_token_span(raw, "save"), raw);
                    err.with_code("E0702").with_help(
                        "Usage: `:history save <file> [selector]`");
                    std::cout << err.format();
                    return HistoryStatus::Error;
                }

                std::vector<std::pair<int, HistoryEntry>> entries;
                if (cmd.range().empty()) {
                    for (int i = 0;
                         i < static_cast<int>(session_history.size());
                         ++i) {
                        if (should_show_entry(session_history[i], cmd))
                            entries.emplace_back(i + 1, session_history[i]);
                    }
                } else {
                    std::vector<int>         indices;
                    std::string              err;
                    std::vector<std::string> commands;
                    for (const auto& entry : session_history)
                        commands.push_back(entry.command);

                    if (!resolve_range(cmd.range(), commands, indices, err)) {
                        MathError math_err(
                            err,
                            Span(raw.find_last_of(" \t") + 1, raw.length()),
                            raw);
                        math_err.with_code("E0701").with_label("invalid range");
                        std::cout << math_err.format();
                        return HistoryStatus::Error;
                    }
                    for (int i : indices) {
                        if (should_show_entry(session_history[i], cmd))
                            entries.emplace_back(i + 1, session_history[i]);
                    }
                }

                if (save_history_to_file(cmd.filepath(), entries)) {
                    std::cout << "  Saved " << entries.size()
                              << " entry(s) to '" << cmd.filepath() << "'\n";
                    return HistoryStatus::Success;
                } else {
                    MathError err("cannot write to '" + cmd.filepath() + "'",
                                  find_token_span(raw, cmd.filepath()),
                                  raw);
                    err.with_code("E0703").with_label(
                        "permission denied or path invalid");
                    std::cout << err.format();
                    return HistoryStatus::Error;
                }
            }

            case HistoryCommand::Action::Clear: {
                // Truncates the persistent history file. No error handling.
                std::ofstream file(get_history_file_path(),
                                   std::ios::trunc | std::ios::out);
                std::cout << "  History cleared\n";
                return HistoryStatus::Info;
            }
            }
            return HistoryStatus::Unknown;
        }

    } // namespace handlers
} // namespace math_solver