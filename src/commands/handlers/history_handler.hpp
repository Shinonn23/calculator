#pragma once

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

        inline bool is_history_noise(const std::string& /*cmd*/) {
            return false;
        }

        inline void append_history_file(const HistoryEntry& entry) {
            std::ofstream file(get_history_file_path(),
                               std::ios::app | std::ios::out);
            if (file.is_open())
                file << entry.timestamp << " [" << to_string(entry.status)
                     << "]\n" << entry.command << "\n";
        }

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

        // Validates indices against history bounds.
        // Indices are 1-based externally, 0-based in out_indices.
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

        // Extracts command strings from history for range validation.
        inline std::vector<std::string>
        extract_commands(const std::vector<HistoryEntry>& history) {
            std::vector<std::string> cmds;
            cmds.reserve(history.size());
            for (const auto& e : history)
                cmds.push_back(e.command);
            return cmds;
        }

        // Computes span covering the selector portion of a history command.
        // Used to highlight the invalid range token in diagnostics.
        inline Span selector_span(const std::string& raw) {
            size_t space = raw.find_first_of(" \t", raw.find(":history"));
            size_t start = (space != std::string::npos)
                               ? raw.find_first_not_of(" \t", space)
                               : raw.length();
            return Span(start, raw.length());
        }

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