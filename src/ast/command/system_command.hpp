#pragma once

#include <string>

#include "command.hpp"
#include "command_visitor.hpp"

namespace math_solver {
    // Represents meta-commands that control the REPL or environment, not user
    // math input.
    // - Type enum must remain in sync with CommandVisitor and any dispatch
    // logic.
    // - SystemCommand is assumed to be terminal in the command pipeline;
    // downstream passes
    //   should not expect further semantic analysis.
    // - The 'raw' string is preserved for diagnostics and round-tripping.
    class SystemCommand : public Command {
        public:
        enum class Type { Exit, Help, Clear, Ls, Unknown };

        private:
        Type type_;

        public:
        SystemCommand(Type type, const std::string& raw)
            : Command(raw), type_(type) {}

        Type type() const { return type_; }

        // Accept must dispatch to CommandVisitor::visit(SystemCommand).
        // Invariant: visitor must handle all SystemCommand::Type variants.
        void accept(CommandVisitor& visitor) const override {
            visitor.visit(*this);
        }
    };
} // namespace math_solver
