#pragma once

#include <optional>
#include <string>

#include "command.hpp"
#include "command_visitor.hpp"

namespace math_solver {
    // Represents a variable-related command in the AST.
    //
    // Invariant: `var_name_` must always be a valid identifier.
    // - For Action::Set, `payload_` may contain an expression or sub-action.
    // - For Action::Unset, `payload_` and `math_action_` must be disengaged.
    //
    // If `math_action_` is set, it indicates a higher-level operation (e.g.,
    // "solve", "expand") that should be interpreted by downstream passes. This
    // allows for future extensibility without proliferating command types.
    //
    // Note: The payload is currently a raw string, but may be replaced with a
    // structured representation (e.g., MathCommandPtr) if command complexity
    // increases.
    //
    // Performance: Avoids heap allocations unless payloads are present.
    class VarCommand : public Command {
        public:
        enum class Action { Set, Unset };

        private:
        Action                     action_;
        std::string                var_name_;
        std::optional<std::string> payload_;
        std::optional<std::string> math_action_;

        public:
        VarCommand(Action act, const std::string& var, const std::string& raw)
            : Command(raw), action_(act), var_name_(var) {}

        // Sets both the high-level math action and its associated payload.
        // Caller must ensure semantic consistency between action and payload.
        void set_payload(const std::string& action,
                         const std::string& payload) {
            math_action_ = action;
            payload_     = payload;
        }

        Action             action() const { return action_; }
        const std::string& var_name() const { return var_name_; }
        bool               has_payload() const { return payload_.has_value(); }
        const std::string& payload() const { return *payload_; }
        const std::string& math_action() const { return *math_action_; }

        // Double-dispatch entry point for visitor pattern.
        // All VarCommand variants must be handled by CommandVisitor.
        void               accept(CommandVisitor& visitor) const override {
            visitor.visit(*this);
        }
    };

} // namespace math_solver
