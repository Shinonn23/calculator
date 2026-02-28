#pragma once

//! # Module — `src/ast/command/command.hpp`
//!
//! Defines `Command`, the abstract base class for all command AST nodes, and
//! the `CommandPtr` ownership alias. All concrete command types (e.g.,
//! `MathCommand`, `VarCommand`) inherit from `Command`. Part of the command
//! layer; command nodes are produced by the command parser and dispatched by
//! `CommandRegistry`.

#include <memory>
#include <string>

#include "command_visitor.hpp"

namespace math_solver {

    class DiagnosticSink;

    /// Abstract base class for all command AST nodes.
    ///
    /// Carries the raw input string and source-location metadata used by
    /// diagnostic messages. Subclasses must implement `accept` to participate
    /// in the `CommandVisitor` double-dispatch mechanism.
    ///
    /// Invariants:
    /// * `source_file_` is always non-empty (defaults to `"<repl>"`).
    /// * `source_line_` is always >= 1 (defaults to `1`).
    class Command {
        protected:
        std::string raw_command_;
        // Tracks the origin of the command for diagnostics and error reporting.
        // Invariants:
        // - source_file_ is always non-empty.
        // - source_line_ is always >= 1.
        std::string source_file_ = "<repl>";
        size_t      source_line_ = 1;

        public:
        /// Construct a `Command` with the given raw input string.
        explicit Command(const std::string& raw) : raw_command_(raw) {}
        virtual ~Command() = default;

        /// Return the original, unmodified input string.
        const std::string& raw_command() const { return raw_command_; }

        /// Return the source file name associated with this command.
        const std::string& source_file() const { return source_file_; }

        /// Return the 1-based source line number for this command.
        size_t             source_line() const { return source_line_; }

        /// Set the source location metadata for diagnostic reporting.
        ///
        /// Must be called before any error reporting that depends on file/line
        /// information. Called by the parser after constructing each command node.
        ///
        /// # Arguments
        ///
        /// * `file` — Non-empty source file path or `"<repl>"`.
        /// * `line` — 1-based line number within `file`.
        void               set_source(const std::string& file, size_t line) {
            source_file_ = file;
            source_line_ = line;
        }

        /// Accept a visitor for double-dispatch execution.
        ///
        /// Subclasses must delegate to `visitor.visit(*this, sink)`.
        ///
        /// # Arguments
        ///
        /// * `visitor` — The command handler implementing `CommandVisitor`.
        /// * `sink`    — Diagnostic sink for reporting errors and warnings.
        virtual void accept(CommandVisitor& visitor,
                            DiagnosticSink& sink) const = 0;
    };

    /// Ownership alias for heap-allocated `Command` nodes.
    using CommandPtr = std::unique_ptr<Command>;

} // namespace math_solver
