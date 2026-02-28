#pragma once

//! # Module — `src/ast/command/command_visitor.hpp`
//!
//! Defines the `CommandVisitor` interface, which provides double-dispatch for
//! all concrete command AST node types. Part of the command layer; the
//! `CommandRegistry` and all command handlers implement this interface.

namespace math_solver {
    class DiagnosticSink;

    /// Pure abstract visitor for all command AST node types.
    ///
    /// Provides a closed set of `visit` overloads, one per concrete command
    /// type. All implementations must define every overload; the absence of
    /// defaults forces explicit handling and prevents silent dispatch failures.
    /// When a new command type is added to the AST, a corresponding `visit`
    /// overload must be added here in lockstep.
    class SystemCommand;
    class VarCommand;
    class MathCommand;
    class EnvCommand;
    class ConfigCommand;
    class LoadCommand;
    class HistoryCommand;
    class RedoCommand;

    class CommandVisitor {
        public:
        virtual ~CommandVisitor() = default;

        /// Dispatch to the handler for a `SystemCommand` node (exit, help, clear, ls).
        virtual void visit(const SystemCommand& cmd, DiagnosticSink& sink)  = 0;

        /// Dispatch to the handler for a `VarCommand` node (variable set/unset).
        virtual void visit(const VarCommand& cmd, DiagnosticSink& sink)     = 0;

        /// Dispatch to the handler for a `MathCommand` node (evaluate, solve, etc.).
        virtual void visit(const MathCommand& cmd, DiagnosticSink& sink)    = 0;

        /// Dispatch to the handler for an `EnvCommand` node (environment management).
        virtual void visit(const EnvCommand& cmd, DiagnosticSink& sink)     = 0;

        /// Dispatch to the handler for a `ConfigCommand` node (config get/set/list).
        virtual void visit(const ConfigCommand& cmd, DiagnosticSink& sink)  = 0;

        /// Dispatch to the handler for a `LoadCommand` node (load a script file).
        virtual void visit(const LoadCommand& cmd, DiagnosticSink& sink)    = 0;

        /// Dispatch to the handler for a `HistoryCommand` node (history operations).
        virtual void visit(const HistoryCommand& cmd, DiagnosticSink& sink) = 0;

        /// Dispatch to the handler for a `RedoCommand` node (redo past commands).
        virtual void visit(const RedoCommand& cmd, DiagnosticSink& sink)    = 0;
    };
} // namespace math_solver
