#pragma once

//! # Module — `src/commands/handlers/history_handler.hpp`
//!
//! Implements history I/O helpers and the top-level `handle_history` dispatcher
//! for `HistoryCommand` nodes (show, show-range, search, save, clear). Persistence
//! uses a plain-text file at the path returned by `get_history_file_path()`.
//! All functions are `inline` and live entirely in this header.

#include "ast/command/history_command.hpp"
#include "ast/command/history_entry.hpp"
#include "diagnostics/kinds/history_errors.hpp"
#include "diagnostics/sink.hpp"
#include "ui/color.hpp"
#include "utils/path_utils.hpp"
#include "utils/string_utils.hpp"

#include "diagnostics/sink.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace math_solver {
    namespace handlers {

        /// Return `true` when `cmd` is a command string that should be suppressed
        /// from history recording.
        ///
        /// Currently always returns `false` (no commands are suppressed). Reserved
        /// for future use (e.g. filtering `:history` itself or empty input).
        ///
        /// # Arguments
        ///
        /// * `cmd` — The raw command string to test.
        inline bool is_history_noise(const std::string& /*cmd*/) {
            return false;
        }

        /// Append `entry` to the persistent history file as two lines.
        ///
        /// The first line is `"<timestamp> [<status>]"` and the second line is the
        /// raw command string. Opens the file in append mode; a missing file is
        /// created automatically. Silently does nothing if the file cannot be opened.
        ///
        /// # Arguments
        ///
        /// * `entry` — The history entry to persist.
        inline void append_history_file(const HistoryEntry& entry) {
            std::ofstream file(get_history_file_path(),
                               std::ios::app | std::ios::out);
            if (file.is_open())
                file << entry.timestamp << " [" << to_string(entry.status)
                     << "]\n" << entry.command << "\n";
        }

        /// Read all persisted history entries from the history file.
        ///
        /// Each entry spans two lines in the file: a header line of the form
        /// `"<timestamp> [<status>]"` followed by a command line. Lines that do
        /// not match the header format are skipped. Returns an empty vector when
        /// the file does not exist or cannot be opened.
        ///
        /// # Returns
        ///
        /// A vector of `HistoryEntry` values in file order (oldest first).
        inline std::vector<HistoryEntry> load_history_file() {
            std::vector<HistoryEntry> history;
            std::ifstream             file(get_history_file_path());
            if (!file.is_open())
                return history;

            std::string line;
            while (std::getline(file, line)) {
                std::string trimmed = trim(line);
                if (trimmed.empty())
                    continue;

                size_t bs = trimmed.find('[');
                size_t be = trimmed.rfind(']');
                if (bs == std::string::npos || be == std::string::npos ||
                    be <= bs)
                    continue;

                HistoryEntry entry;
                entry.timestamp = trim(trimmed.substr(0, bs));
                entry.status    = parse_history_status(
                    trim(trimmed.substr(bs + 1, be - bs - 1)));

                if (std::getline(file, line)) {
                    entry.command = trim(line);
                    history.push_back(entry);
                }
            }
            return history;
        }

        /// Validate and convert 1-based range indices into 0-based vector offsets.
        ///
        /// Each element of `range` is a 1-based history index. The function checks
        /// that every index is within `[1, history.size()]` and writes the
        /// corresponding 0-based offset into `out_indices`.
        ///
        /// # Arguments
        ///
        /// * `range`       — 1-based index list to validate (e.g. from `:redo 1,3,5-8`).
        /// * `history`     — Command string vector whose size defines the valid bound.
        /// * `out_indices` — Receives the validated 0-based offsets; appended to, not cleared.
        /// * `out_error`   — Receives a human-readable error message on failure.
        ///
        /// # Returns
        ///
        /// `true` when all indices are valid; `false` when any index is out of range.
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

        /// Format and emit a sequence of history entries to `sink`.
        ///
        /// Each entry is printed as one line: `[<1-based-index>] <timestamp> [ok|error|warn|info]  <command>`.
        /// Status indicators are ANSI-coloured (green/red/yellow/dim).
        ///
        /// # Arguments
        ///
        /// * `entries` — Pairs of (1-based index, `HistoryEntry`) to display.
        /// * `sink`    — Diagnostic sink that receives the formatted output string.
        inline void print_history_entries(
            const std::vector<std::pair<int, HistoryEntry>>& entries,
            DiagnosticSink&                                  sink) {
            std::ostringstream oss;
            for (const auto& [idx, entry] : entries) {
                oss << "  " << ansi::dim << "[" << idx << "] "
                    << entry.timestamp << ansi::reset;
                if (entry.is_success())
                    oss << ansi::green << " [ok]    ";
                else if (entry.is_error())
                    oss << ansi::red << " [error] ";
                else if (entry.is_warning())
                    oss << ansi::yellow << " [warn]  ";
                else
                    oss << ansi::dim << " [info]  ";
                oss << ansi::reset << entry.command << "\n";
            }
            sink.push_output(oss.str());
        }

        /// Write `entries` to `filepath` as a runnable `.msl` script.
        ///
        /// Writes one raw command string per line. The file is created or
        /// truncated. Silently returns `false` if the file cannot be opened.
        ///
        /// # Arguments
        ///
        /// * `filepath` — Destination file path.
        /// * `entries`  — Pairs of (index, `HistoryEntry`); only the command field is written.
        ///
        /// # Returns
        ///
        /// `true` when the file is written successfully; `false` on I/O failure.
        inline bool save_history_to_file(
            const std::string&                               filepath,
            const std::vector<std::pair<int, HistoryEntry>>& entries) {
            std::ofstream file(filepath);
            if (!file.is_open())
                return false;
            for (const auto& [_, entry] : entries)
                file << entry.command << "\n";
            return true;
        }

        /// Return `true` when `entry` passes the filter flags set on `cmd`.
        ///
        /// If no filter flags are active (`cmd.has_any_flag()` is `false`),
        /// every entry passes. Otherwise only entries whose status matches one of
        /// the set flags (`Success`, `Errors`, `Warning`, `Info`) pass.
        ///
        /// # Arguments
        ///
        /// * `entry` — The history entry to test.
        /// * `cmd`   — The history command whose active filter flags are tested.
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

        /// Extract the raw command string from each `HistoryEntry` in `history`.
        ///
        /// # Returns
        ///
        /// A vector of command strings in the same order as `history`, suitable
        /// for passing to `resolve_range`.
        inline std::vector<std::string>
        extract_commands(const std::vector<HistoryEntry>& history) {
            std::vector<std::string> cmds;
            cmds.reserve(history.size());
            for (const auto& e : history)
                cmds.push_back(e.command);
            return cmds;
        }

        /// Compute the `Span` that covers the selector portion of a history command.
        ///
        /// Finds the first whitespace after `:history` and returns a span from
        /// the start of the next non-whitespace character to the end of `raw`.
        /// Used to highlight the invalid range token in diagnostics.
        ///
        /// # Arguments
        ///
        /// * `raw` — The full raw command string (e.g. `":history 3-5"`).
        ///
        /// # Returns
        ///
        /// A `Span` covering the selector token(s); spans the full string end
        /// if no whitespace is found after `:history`.
        inline Span selector_span(const std::string& raw) {
            size_t space = raw.find_first_of(" \t", raw.find(":history"));
            size_t start = (space != std::string::npos)
                               ? raw.find_first_not_of(" \t", space)
                               : raw.length();
            return Span(start, raw.length());
        }

        /// Dispatch a `HistoryCommand` to the appropriate sub-handler.
        ///
        /// Routes `cmd.action()` to:
        /// - `Show`      — Displays the last `cmd.limit()` entries (default 20)
        ///   filtered by `should_show_entry`.
        /// - `ShowRange` — Displays entries selected by `cmd.range()`.
        /// - `Search`    — Displays entries whose command contains `cmd.pattern()`.
        /// - `Save`      — Writes selected entries to `cmd.filepath()` as a script.
        /// - `Clear`     — Truncates the on-disk history file.
        /// - `Unknown`   — Emits `unknown_history_subcommand` diagnostic.
        ///
        /// # Arguments
        ///
        /// * `cmd`             — The history command to execute.
        /// * `session_history` — The full in-memory history for this session.
        /// * `sink`            — Diagnostic sink for errors and output.
        ///
        /// # Returns
        ///
        /// `HistoryStatus::Info` for `Show` (empty), `Clear`, and `Show` when
        /// all entries are filtered out. `HistoryStatus::Success` for all other
        /// successful actions. `HistoryStatus::Error` on missing arguments, invalid
        /// ranges, file write failures, or `Unknown` action.
        ///
        /// # Errors
        ///
        /// Pushes `history_missing_arg` when `Search` has no pattern or `Save` has
        /// no filepath. Pushes `history_range_error` when `ShowRange` or `Save` range
        /// is invalid. Pushes `history_write_error` on file I/O failure. Pushes
        /// `unknown_history_subcommand` for `Unknown` action.
        inline HistoryStatus
        handle_history(const HistoryCommand&            cmd,
                       const std::vector<HistoryEntry>& session_history,
                       DiagnosticSink&                  sink) {

            const std::string& raw  = cmd.raw_command();
            const std::string& file = cmd.source_file();
            size_t             line = cmd.source_line();

            switch (cmd.action()) {

            case HistoryCommand::Action::Show: {
                if (session_history.empty()) {
                    sink.push_output("  No history\n");
                    return HistoryStatus::Info;
                }
                int total = static_cast<int>(session_history.size());
                int start =
                    (cmd.limit() == 0) ? 0 : std::max(0, total - cmd.limit());

                std::vector<std::pair<int, HistoryEntry>> entries;
                for (int i = start; i < total; ++i)
                    if (should_show_entry(session_history[i], cmd))
                        entries.emplace_back(i + 1, session_history[i]);

                print_history_entries(entries, sink);
                return HistoryStatus::Success;
            }

            case HistoryCommand::Action::ShowRange: {
                std::vector<int> indices;
                std::string      err;
                if (!resolve_range(cmd.range(),
                                   extract_commands(session_history), indices,
                                   err)) {
                    sink.push(errors::history_range_error(
                        raw, err, selector_span(raw), file, line));
                    return HistoryStatus::Error;
                }
                std::vector<std::pair<int, HistoryEntry>> entries;
                for (int i : indices)
                    if (should_show_entry(session_history[i], cmd))
                        entries.emplace_back(i + 1, session_history[i]);

                print_history_entries(entries, sink);
                return HistoryStatus::Success;
            }

            case HistoryCommand::Action::Search: {
                if (cmd.pattern().empty()) {
                    sink.push(errors::history_missing_arg(
                        raw, "search", "`:history search <pattern>`", file,
                        line));
                    return HistoryStatus::Error;
                }
                std::vector<std::pair<int, HistoryEntry>> matches;
                for (int i = 0; i < static_cast<int>(session_history.size());
                     ++i)
                    if (session_history[i].command.find(cmd.pattern()) !=
                            std::string::npos &&
                        should_show_entry(session_history[i], cmd))
                        matches.emplace_back(i + 1, session_history[i]);

                if (matches.empty()) {
                    std::ostringstream oss;
                    oss << "  No matches found for '" << cmd.pattern() << "'"
                        << (cmd.has_any_flag() ? " with current filters" : "")
                        << "\n";
                    sink.push_output(oss.str());
                    return HistoryStatus::Info;
                }
                print_history_entries(matches, sink);
                return HistoryStatus::Success;
            }

            case HistoryCommand::Action::Save: {
                if (cmd.filepath().empty()) {
                    sink.push(errors::history_missing_arg(
                        raw, "save", "`:history save <file> [selector]`", file,
                        line));
                    return HistoryStatus::Error;
                }

                std::vector<std::pair<int, HistoryEntry>> entries;
                auto cmds = extract_commands(session_history);

                if (cmd.range().empty()) {
                    for (int i = 0;
                         i < static_cast<int>(session_history.size()); ++i)
                        if (should_show_entry(session_history[i], cmd))
                            entries.emplace_back(i + 1, session_history[i]);
                } else {
                    std::vector<int> indices;
                    std::string      err;
                    if (!resolve_range(cmd.range(), cmds, indices, err)) {
                        sink.push(errors::history_range_error(
                            raw, err,
                            Span(raw.find_last_of(" \t") + 1, raw.length()),
                            file, line));
                        return HistoryStatus::Error;
                    }
                    for (int i : indices)
                        if (should_show_entry(session_history[i], cmd))
                            entries.emplace_back(i + 1, session_history[i]);
                }

                if (!save_history_to_file(cmd.filepath(), entries)) {
                    sink.push(errors::history_write_error(raw, cmd.filepath(),
                                                          file, line));
                    return HistoryStatus::Error;
                }
                std::ostringstream oss;
                oss << "  Saved " << entries.size()
                    << (cmd.has_any_flag() ? " filtered" : "")
                    << " entry(s) to '" << cmd.filepath() << "'\n";
                sink.push_output(oss.str());
                return HistoryStatus::Success;
            }

            case HistoryCommand::Action::Clear: {
                std::ofstream file(get_history_file_path(),
                                   std::ios::trunc | std::ios::out);
                sink.push_output("  History cleared\n");
                return HistoryStatus::Info;
            }

            case HistoryCommand::Action::Unknown: {
                const std::string& sub = cmd.raw_command();
                sink.push(
                    errors::unknown_history_subcommand(raw, sub, file, line));
                return HistoryStatus::Error;
            }
            }
            return HistoryStatus::Unknown;
        }

    } // namespace handlers
} // namespace math_solver