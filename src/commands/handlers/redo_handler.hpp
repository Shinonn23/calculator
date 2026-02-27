#pragma once

#include "ast/command/history_entry.hpp"
#include "ast/command/redo_command.hpp"
#include "commands/handlers/history_handler.hpp"
#include "diagnostics/diagnostic.hpp"
#include "diagnostics/kinds/math_errors.hpp"
#include "diagnostics/sink.hpp"
#include "ui/color.hpp"

#include <iostream>
#include <string>
#include <vector>

namespace math_solver {
    namespace handlers {

        // Redo handler: re-dispatches commands from session history.
        //
        // Invariants:
        // - `dispatch_fn` must not mutate `session_history` directly; all
        // mutations
        //   are expected to go through the main event loop to preserve history
        //   integrity.
        // - If `cmd.range()` is empty, only the most recent command is
        // re-executed.
        // - Each redo is treated as a new history entry by the main loop, so
        // repeated
        //   redos will accumulate in history.
        //
        // Correctness:
        // - The function assumes that `session_history` is append-only and that
        // indices
        //   are stable for the duration of this call.
        // - If `resolve_range` fails, an error is reported and no commands are
        // dispatched.
        // - There is no feedback channel from `dispatch_fn` to indicate
        // success/failure
        //   of individual commands; this is a deliberate design to keep the
        //   handler stateless with respect to command execution outcomes.
        //
        // Edge cases:
        // - If history is empty, returns early with an informational status.
        // - If the range is invalid, emits a diagnostic with precise span info.
        //
        // Performance:
        // - Linear scan over `session_history` to extract command strings; cost
        // is
        //   negligible unless history is extremely large.
        template <typename DispatchFn>
        inline HistoryStatus
        handle_redo(const RedoCommand&               cmd,
                    const std::vector<HistoryEntry>& session_history,
                    DispatchFn dispatch_fn, DiagnosticSink& sink) {
            const std::string& raw = cmd.raw_command();
            if (session_history.empty()) {
                std::cout << "  No history to redo\n";
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
                            raw)
                            .with_label("invalid range");
                    d.code = "E0801";
                    sink.push(d);
                    return HistoryStatus::Error;
                }
            }

            bool all_success = true;
            for (int i : indices) {
                const std::string& entry = session_history[i].command;
                std::cout << ansi::dim << "  >> " << entry << ansi::reset
                          << "\n";
                // No mechanism to detect dispatch_fn failure; assumes success.
                dispatch_fn(entry);
            }
            return all_success ? HistoryStatus::Success : HistoryStatus::Error;
        }

    } // namespace handlers
} // namespace math_solver