#pragma once

//! # Module — `src/commands/handlers/load_handler.hpp`
//!
//! Implements `handle_load` — the handler for `LoadCommand` nodes (`:load
//! <file.msl>`). Script execution is fully delegated to `Runner::run_script`;
//! this module only checks the post-run error flag and surfaces a diagnostic
//! when the script fails.

#include "ast/command/history_entry.hpp"
#include "ast/command/load_command.hpp"
#include "diagnostics/diagnostic.hpp"
#include "diagnostics/sink.hpp"
#include "ui/repl/runner.hpp"

namespace math_solver {
    namespace handlers {

        /// Execute a `:load <file>` command by delegating to `runner.run_script`.
        ///
        /// Invokes `runner.run_script` with the file path and flags from `cmd`.
        /// After the call, checks `runner.last_script_had_errors()` and emits an
        /// E0900 diagnostic if any commands in the script produced errors.
        /// The `Runner` itself manages all file I/O and command dispatching;
        /// this handler only bridges the command AST to the runner API.
        ///
        /// # Arguments
        ///
        /// * `cmd`    — The `:load` command node; supplies `filepath()` and `flags()`.
        /// * `runner` — The script runner; must be fully initialised before this call.
        /// * `sink`   — Diagnostic sink that receives the E0900 error on script failure.
        ///
        /// # Returns
        ///
        /// `HistoryStatus::Info` when the script completes without errors.
        /// `HistoryStatus::Error` when `runner.last_script_had_errors()` is `true`.
        ///
        /// # Errors
        ///
        /// Pushes E0900 ("script '…' failed") annotated with `cmd.source_file()`
        /// and `cmd.source_line()` when the script execution results in errors.
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