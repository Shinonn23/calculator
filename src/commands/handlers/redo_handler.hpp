#pragma once

//! # Module — `src/commands/handlers/redo_handler.hpp`
//!
//! Implements `handle_redo` — the handler for `RedoCommand` nodes (`:redo`,
//! `:redo <n>`, `:redo <selector>`). Re-dispatches one or more history entries
//! by calling a caller-supplied `dispatch_fn`. The function is a function
//! template to keep the handler decoupled from `HandlerRegistry`.

#include "ast/command/history_entry.hpp"
#include "ast/command/redo_command.hpp"
#include "commands/handlers/history_handler.hpp"
#include "diagnostics/diagnostic.hpp"
#include "diagnostics/kinds/math_errors.hpp"
#include "diagnostics/sink.hpp"
#include "ui/color.hpp"

#include <string>
#include <vector>

namespace math_solver {
    namespace handlers {

        /// Re-execute one or more commands from `session_history` by calling `dispatch_fn`.
        ///
        /// When `cmd.range()` is empty, re-executes the most recent history entry.
        /// Otherwise, parses the range selector via `resolve_range` and dispatches
        /// each selected entry in order. Each re-executed command is echoed to
        /// `sink` with a dim `">> "` prefix before dispatch.
        ///
        /// Each redo call is itself recorded as a new history entry by the main
        /// loop; `dispatch_fn` must not mutate `session_history` directly.
        /// There is no feedback channel from `dispatch_fn` for per-command
        /// success/failure — this is intentional to keep the handler stateless.
        ///
        /// # Arguments
        ///
        /// * `cmd`             — The `:redo` command node; supplies `range()` and source info.
        /// * `session_history` — Immutable snapshot of the current session history;
        ///   assumed append-only and index-stable for the duration of this call.
        /// * `dispatch_fn`     — Callable `(const std::string& raw) -> void` that
        ///   re-parses and dispatches the given command string.
        /// * `sink`            — Diagnostic sink for errors and echoed command output.
        ///
        /// # Returns
        ///
        /// `HistoryStatus::Info` when history is empty.
        /// `HistoryStatus::Success` when all selected commands are dispatched.
        /// `HistoryStatus::Error` when the range selector is invalid (E0801).
        ///
        /// # Errors
        ///
        /// Pushes E0801 with an `"invalid range"` label when `resolve_range` fails
        /// to parse `cmd.range()`.
        template <typename DispatchFn>
        inline HistoryStatus
        handle_redo(const RedoCommand&               cmd,
                    const std::vector<HistoryEntry>& session_history,
                    DispatchFn dispatch_fn, DiagnosticSink& sink) {
            const std::string& raw = cmd.raw_command();
            if (session_history.empty()) {
                sink.push_output("  No history to redo\n");
                return HistoryStatus::Info;
            }

            std::vector<int>         indices;
            std::string              err;

            std::vector<std::string> commands;
            for (const auto& entry : session_history)
                commands.push_back(entry.command);

            if (cmd.range().empty()) {
                indices.push_back(static_cast<int>(session_history.size()) - 1);
            } else {
                if (!resolve_range(cmd.range(), commands, indices, err)) {
                    Diagnostic d =
                        errors::math(
                            err,
                            Span(raw.find_last_of(" \t") + 1, raw.length()),
                            raw, cmd.source_file(), cmd.source_line())
                            .with_label("invalid range");
                    d.code = "E0801";
                    sink.push(d);
                    return HistoryStatus::Error;
                }
            }

            bool all_success = true;
            for (int i : indices) {
                const std::string& entry = session_history[i].command;
                sink.push_output(std::string(ansi::dim) + "  >> " + entry +
                                 ansi::reset + "\n");
                // No mechanism to detect dispatch_fn failure; assumes success.
                dispatch_fn(entry);
            }
            return all_success ? HistoryStatus::Success : HistoryStatus::Error;
        }

    } // namespace handlers
} // namespace math_solver