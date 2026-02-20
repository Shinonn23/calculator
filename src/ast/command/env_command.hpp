#ifndef ENV_COMMAND
#define ENV_COMMAND
#include <string>
#include <vector>

#include "command.hpp"
#include "command_visitor.hpp"

namespace math_solver {
    // Represents an environment-related command in the AST.
    //
    // Invariant: `action_` must always be a valid Action variant.
    // - For Action::Save, `vars_to_save_` may be non-empty to indicate a subset
    // of variables.
    // - `target_env_` is the identifier for the environment; must be non-empty
    // for all actions except List.
    //
    // This type is central to environment management and is consumed by the
    // command visitor. Any changes to the set of actions or their semantics
    // must be coordinated with the command visitor logic.
    class EnvCommand : public Command {
        public:
        enum class Action { Show, List, Load, Save, New, Delete };

        private:
        Action                   action_;
        std::string              target_env_;
        std::vector<std::string> vars_to_save_;

        public:
        EnvCommand(Action             act,
                   const std::string& target,
                   const std::string& raw)
            : Command(raw), action_(act), target_env_(target) {}

        // Overwrites the set of variables to be saved.
        // Only meaningful for Action::Save; ignored otherwise.
        void set_vars_to_save(const std::vector<std::string>& vars) {
            vars_to_save_ = vars;
        }

        Action             action() const { return action_; }
        const std::string& target_env() const { return target_env_; }
        const std::vector<std::string>& vars_to_save() const {
            return vars_to_save_;
        }

        // Double-dispatch entry point for command visitors.
        // All logic for handling EnvCommand must be implemented in
        // CommandVisitor::visit(EnvCommand&).
        void accept(CommandVisitor& visitor) const override {
            visitor.visit(*this);
        }
    };
} // namespace math_solver

#endif