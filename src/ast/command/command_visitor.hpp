#ifndef CLI_VISITOR
#define CLI_VISITOR

namespace math_solver {

    // The CommandVisitor interface abstracts over all supported command types.
    //
    // - Maintainers: When adding new command types, ensure this visitor is
    // updated
    //   to avoid silent omissions in command dispatch.
    // - The use of forward declarations here is intentional to minimize header
    // dependencies
    //   and reduce incremental build times. Do not include full command headers
    //   unless absolutely necessary.
    // - All visit methods are required to be implemented by consumers; default
    // implementations
    //   are intentionally omitted to force explicit handling of each command
    //   variant.
    class SystemCommand;
    class VarCommand;
    class MathCommand;
    class EnvCommand;
    class ConfigCommand;

    class CommandVisitor {
        public:
        virtual ~CommandVisitor()                    = default;

        virtual void visit(const SystemCommand& cmd) = 0;
        virtual void visit(const VarCommand& cmd)    = 0;
        virtual void visit(const MathCommand& cmd)   = 0;
        virtual void visit(const EnvCommand& cmd)    = 0;
        virtual void visit(const ConfigCommand& cmd) = 0;
    };
} // namespace math_solver

#endif // MATH_SOLVER_CLI_VISITOR_HPP