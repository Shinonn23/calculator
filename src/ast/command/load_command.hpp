#pragma once

//! # Module — `src/ast/command/load_command.hpp`
//!
//! Defines `LoadCommand`, the AST node for the `:load` command that reads an
//! external script file into the solver context. Part of the command layer;
//! produced by the load subparser and dispatched via `CommandRegistry`.

#include "command.hpp"
#include <string>

namespace math_solver {

    /// AST node for loading an external script file into the solver context.
    ///
    /// The `Flags` struct carries behavioral modifiers supplied by the user:
    /// * `dry_run`     — Simulate the load without side effects.
    /// * `silent`      — Suppress user-facing output; intended for batch use.
    /// * `strict`      — Treat warnings as errors during loading.
    /// * `no_rollback` — Do not undo partial context changes on failure.
    /// * `env`         — Optional environment context name for variable scoping.
    ///
    /// Invariant: `filepath_` must be a valid, non-empty path when the command
    /// is dispatched. Flag values are set by the parser; no runtime validation
    /// is performed in this node.
    class LoadCommand : public Command {
        public:
        /// Behavioral modifiers for the load operation.
        struct Flags {
            bool        dry_run     = false;
            bool        silent      = false;
            bool        strict      = false;
            bool        no_rollback = false;
            std::string env         = "";
        };

        /// Construct a `LoadCommand` with a file path and flags.
        ///
        /// # Arguments
        ///
        /// * `filepath` — Non-empty path to the script file to load.
        /// * `flags`    — Behavioral modifiers for the load operation.
        /// * `raw`      — The full raw input line for diagnostics.
        LoadCommand(const std::string& filepath, const Flags& flags,
                    const std::string& raw)
            : Command(raw), filepath_(filepath), flags_(flags) {}

        /// Return the path of the script file to load.
        const std::string& filepath() const { return filepath_; }

        /// Return the behavioral flags for this load operation.
        const Flags&       flags() const { return flags_; }

        /// Dispatch to `CommandVisitor::visit(const LoadCommand&, DiagnosticSink&)`.
        void               accept(CommandVisitor& visitor,
                                  DiagnosticSink& sink) const override {
            visitor.visit(*this, sink);
        }

        private:
        std::string filepath_;
        Flags       flags_;
    };

} // namespace math_solver
