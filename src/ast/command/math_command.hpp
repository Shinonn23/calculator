#ifndef MATH_COMMAND
#define MATH_COMMAND
#include <string>
#include <vector>

#include "command.hpp"
#include "command_visitor.hpp"

namespace math_solver {
    class MathCommand : public Command {
        public:
        // Represents the high-level operation requested by the user.
        // The set of variants is expected to remain stable; adding new ones
        // may require changes in downstream consumers (e.g., CommandVisitor).
        enum class Type { Evaluate, Solve, Simplify, Expand, Factor };

        private:
        Type                     type_;
        std::string              payload_;

        // These fields encode optional flags that alter the semantics of the
        // command.
        // - specific_vars_ is only populated for commands that support variable
        // targeting.
        // - isolated_ and as_fraction_ are mutually orthogonal; both may be
        // set. Invariant: specific_vars_ is empty unless the command type
        // supports variable lists.
        std::vector<std::string> specific_vars_;
        bool                     isolated_    = false;
        bool                     as_fraction_ = false;

        public:
        // The raw string is preserved for diagnostics and round-tripping.
        MathCommand(Type               type,
                    const std::string& payload,
                    const std::string& raw)
            : Command(raw), type_(type), payload_(payload) {}

        // set_flags must be called before execution if any flags are present in
        // the input. The default values correspond to the absence of flags.
        void set_flags(bool                            isolated,
                       bool                            fraction,
                       const std::vector<std::string>& vars = {}) {
            isolated_      = isolated;
            as_fraction_   = fraction;
            specific_vars_ = vars;
        }

        Type               type() const { return type_; }
        const std::string& payload() const { return payload_; }
        bool               isolated() const { return isolated_; }
        bool               as_fraction() const { return as_fraction_; }
        const std::vector<std::string>& specific_vars() const {
            return specific_vars_;
        }

        // Double-dispatch entry point; required for integration with the
        // visitor framework. All MathCommand variants must be handled by
        // CommandVisitor::visit.
        void accept(CommandVisitor& visitor) const override {
            visitor.visit(*this);
        }
    };
} // namespace math_solver

#endif