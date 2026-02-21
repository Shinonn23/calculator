#pragma once

#include <string>
#include <vector>

#include "command.hpp"
#include "command_visitor.hpp"

namespace math_solver {

    // EnvCommand encodes environment management operations in the AST.
    // - Action enum must remain in sync with
    // CommandVisitor::visit(EnvCommand&).
    // - Invariants:
    //   * For Move/Copy, both source_env_ and target_env_ must be non-empty.
    //   * For Save, vars_to_save_ may be empty (save all) or non-empty
    //   (subset).
    //   * For Move/Copy with --vars, vars_to_save_ is the subject set, and
    //   target_env_ is required.
    //   * target_env_ is required for all actions except List and Show.
    // - Any semantic changes here must be coordinated with downstream consumers
    // (e.g., command visitor, environment store).
    // - This type is performance-insensitive; construction is infrequent and
    // only in command parsing.

    class EnvCommand : public Command {
        public:
        enum class Action { Show, List, Load, Save, New, Delete, Move, Copy, Unknown };

        struct Flags {
            // When true, Move/Copy operates on a subset of variables (see
            // vars_to_save_). to_env is only meaningful in this mode.
            bool        vars_mode = false;
            std::string to_env;
        };

        private:
        Action                   action_;
        std::string              source_env_;
        std::string              target_env_;
        std::vector<std::string> vars_to_save_;
        Flags                    flags_;

        public:
        EnvCommand(Action             act,
                   const std::string& target,
                   const std::string& raw)
            : Command(raw), action_(act), target_env_(target) {}

        void set_vars_to_save(const std::vector<std::string>& vars) {
            vars_to_save_ = vars;
        }

        void   set_source_env(const std::string& src) { source_env_ = src; }

        void   set_flags(const Flags& flags) { flags_ = flags; }

        Action action() const { return action_; }
        const std::string& source_env() const { return source_env_; }
        const std::string& target_env() const { return target_env_; }
        const std::vector<std::string>& vars_to_save() const {
            return vars_to_save_;
        }
        const Flags& flags() const { return flags_; }

        // Double-dispatch entry point. All semantic handling is delegated to
        // CommandVisitor::visit(EnvCommand&). Any changes to the structure or
        // invariants here must be reflected in the visitor logic.
        void         accept(CommandVisitor& visitor) const override {
            visitor.visit(*this);
        }
    };

} // namespace math_solver
