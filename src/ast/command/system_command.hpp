#pragma once

//! # Module — `src/ast/command/system_command.hpp`
//!
//! Defines `SystemCommand`, the AST node for meta-commands that control the
//! REPL or environment rather than performing math operations (exit, help,
//! clear, ls). Part of the command layer; produced by the system subparser and
//! dispatched via `CommandRegistry`.

#include <string>

#include "command.hpp"
#include "command_visitor.hpp"

namespace math_solver {
    /// AST node for a REPL or environment meta-command.
    ///
    /// `SystemCommand` is terminal in the command pipeline; downstream passes
    /// should not expect further semantic analysis beyond dispatch. The raw
    /// input string is preserved in the base `Command` for diagnostics.
    ///
    /// Invariant: visitors must handle all `Type` variants, including
    /// `Unknown`.
    class SystemCommand : public Command {
        public:
        /// The meta-command type.
        enum class Type { Exit, Help, Clear, Ls, Unknown };

        private:
        Type type_;

        public:
        /// Construct a `SystemCommand` of the given type.
        ///
        /// # Arguments
        ///
        /// * `Type` — The meta-command kind.
        /// * `raw`  — The full raw input line for diagnostics.
        SystemCommand(Type type, const std::string& raw)
            : Command(raw), type_(type) {}

        /// Return the meta-command type.
        Type type() const { return type_; }

        /// Dispatch to `CommandVisitor::visit(const SystemCommand&,
        /// DiagnosticSink&)`.
        ///
        /// The visitor must handle all `SystemCommand::Type` variants.
        void accept(CommandVisitor& visitor,
                    DiagnosticSink& sink) const override {
            visitor.visit(*this, sink);
        }
    };
} // namespace math_solver
