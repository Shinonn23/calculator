#pragma once

//! # Module — `src/ast/command/redo_command.hpp`
//!
//! Defines `RedoCommand`, the AST node for the `:redo` command that replays
//! one or more previously executed commands from the undo/redo stack.

#include "command.hpp"
#include <vector>

namespace math_solver {
    /// AST node for replaying previously executed commands.
    ///
    /// An empty `range_` means redo the single most-recent command. A
    /// non-empty `range_` specifies a set of command indices to replay,
    /// interpreted consistently with the undo/redo stack semantics elsewhere
    /// in the system.
    class RedoCommand : public Command {
        // Invariant: range_ is either empty (indicating a redo of the last
        // command) or contains indices specifying a contiguous range of
        // commands to redo. The interpretation of these indices is expected to
        // be consistent with the undo/redo stack semantics elsewhere in the
        // system.
        std::vector<int> range_;

        public:
        /// Construct a `RedoCommand` from the raw input line.
        RedoCommand(const std::string& raw) : Command(raw) {}

        /// Set the indices of the commands to replay.
        ///
        /// An empty vector means redo the single most-recent command.
        ///
        /// # Arguments
        ///
        /// * `range` — Ordered list of command indices to replay.
        void set_range(const std::vector<int>& range) { range_ = range; }

        /// Return the list of command indices to replay (empty means last one).
        const std::vector<int>& range() const { return range_; }

        /// Dispatch to `CommandVisitor::visit(const RedoCommand&, DiagnosticSink&)`.
        ///
        /// The visitor must not mutate this node during dispatch.
        void                    accept(CommandVisitor& visitor,
                                       DiagnosticSink& sink) const override {
            visitor.visit(*this, sink);
        }
    };
} // namespace math_solver
