#pragma once

namespace math_solver {
    class DiagnosticSink;

    // CommandVisitor provides a closed set of visit methods for all supported
    // command types.
    //
    // - All visit methods must be implemented; omitting a handler will result
    // in a compile error.
    // - Forward declarations are used to minimize header dependencies and
    // reduce build times.
    // - When introducing a new command type, CommandVisitor must be updated in
    // lockstep to avoid
    //   silent dispatch failures. This invariant is relied upon by the command
    //   dispatch mechanism.
    // - No default implementations are provided to force explicit handling of
    // each command variant.
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

        virtual void visit(const SystemCommand& cmd, DiagnosticSink& sink)  = 0;
        virtual void visit(const VarCommand& cmd, DiagnosticSink& sink)     = 0;
        virtual void visit(const MathCommand& cmd, DiagnosticSink& sink)    = 0;
        virtual void visit(const EnvCommand& cmd, DiagnosticSink& sink)     = 0;
        virtual void visit(const ConfigCommand& cmd, DiagnosticSink& sink)  = 0;
        virtual void visit(const LoadCommand& cmd, DiagnosticSink& sink)    = 0;
        virtual void visit(const HistoryCommand& cmd, DiagnosticSink& sink) = 0;
        virtual void visit(const RedoCommand& cmd, DiagnosticSink& sink)    = 0;
    };
} // namespace math_solver
