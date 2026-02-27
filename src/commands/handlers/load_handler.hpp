#pragma once

#include "ast/command/history_entry.hpp"
#include "ast/command/load_command.hpp"
#include "diagnostics/diagnostic.hpp"
#include "diagnostics/sink.hpp"
#include "ui/repl/runner.hpp"

namespace math_solver {
    namespace handlers {

        // Handles the 'load' command by delegating script execution to the REPL
        // runner.
        // - Assumes 'cmd.filepath()' is a valid, accessible path; error
        // handling is deferred to 'runner.run_script'.
        // - The returned HistoryStatus::Info signals that this command does not
        // mutate solver state directly,
        //   but may have side effects via script execution.
        // - Invariant: Runner must be in a consistent state before and after
        // script execution.
        // - Any changes to script loading semantics must be coordinated with
        // the REPL runner's state management.
        inline HistoryStatus handle_load(const LoadCommand& cmd, Runner& runner,
                                         DiagnosticSink& sink) {
            runner.run_script(cmd.filepath(), cmd.flags());

            if (runner.last_script_had_errors()) {
                Diagnostic d =
                    Diagnostic::make("script '" + cmd.filepath() + "' failed",
                                     "E0900", {}, cmd.raw_command(),
                                     "nested script error")
                        .with_location(cmd.source_file(), cmd.source_line());
                sink.push(d);
                return HistoryStatus::Error;
            }

            return HistoryStatus::Info;
        }

    } // namespace handlers
} // namespace math_solver