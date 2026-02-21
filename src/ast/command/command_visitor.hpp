#pragma once

namespace math_solver {

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
        virtual ~CommandVisitor()                     = default;

        virtual void visit(const SystemCommand& cmd)  = 0;
        virtual void visit(const VarCommand& cmd)     = 0;
        virtual void visit(const MathCommand& cmd)    = 0;
        virtual void visit(const EnvCommand& cmd)     = 0;
        virtual void visit(const ConfigCommand& cmd)  = 0;
        virtual void visit(const LoadCommand& cmd)    = 0;
        virtual void visit(const HistoryCommand& cmd) = 0;
        virtual void visit(const RedoCommand& cmd)    = 0;
    };
} // namespace math_solver
