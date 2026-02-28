#pragma once

//! # Module — `src/ast/command/env_command.hpp`
//!
//! Defines `EnvCommand`, the AST node for environment management commands
//! (show, list, load, save, new, delete, move, copy). Part of the command
//! layer; produced by the env subparser and dispatched via `CommandRegistry`.

#include <string>
#include <vector>

#include "command.hpp"
#include "command_visitor.hpp"

namespace math_solver {

    /// AST node for an environment management command.
    ///
    /// Encodes the operation (`Action`), target/source environment names, an
    /// optional variable subset (`vars_to_save_`), and behavioral flags.
    ///
    /// Invariants:
    /// * For `Move`/`Copy`, both `source_env_` and `target_env_` must be
    ///   non-empty before dispatching.
    /// * For `Save`, `vars_to_save_` may be empty (save all) or non-empty
    ///   (save subset).
    /// * `target_env_` is required for all actions except `List` and `Show`.
    class EnvCommand : public Command {
        public:
        /// The environment operation to perform.
        enum class Action {
            Show,
            List,
            Load,
            Save,
            New,
            Delete,
            Move,
            Copy,
            Unknown
        };

        /// Behavioral modifiers for `Move` and `Copy` operations.
        ///
        /// When `vars_mode` is true, the operation applies only to the
        /// variable subset in `vars_to_save_`. `to_env` names the destination
        /// environment in that mode.
        struct Flags {
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
        /// Construct an `EnvCommand` with a target environment and raw input.
        ///
        /// # Arguments
        ///
        /// * `act`    — The environment operation.
        /// * `target` — The primary target environment name (may be empty for
        ///              `List`/`Show`).
        /// * `raw`    — The full raw input line for diagnostics.
        EnvCommand(Action act, const std::string& target,
                   const std::string& raw)
            : Command(raw), action_(act), target_env_(target) {}

        /// Set the variable subset to include in `Save`, `Move`, or `Copy`.
        void set_vars_to_save(const std::vector<std::string>& vars) {
            vars_to_save_ = vars;
        }

        /// Set the source environment name for `Move` and `Copy` operations.
        void   set_source_env(const std::string& src) { source_env_ = src; }

        /// Set the behavioral flags for this command.
        void   set_flags(const Flags& flags) { flags_ = flags; }

        /// Return the environment operation kind.
        Action action() const { return action_; }

        /// Return the source environment name (relevant for `Move`/`Copy`).
        const std::string& source_env() const { return source_env_; }

        /// Return the target environment name.
        const std::string& target_env() const { return target_env_; }

        /// Return the list of variable names to include (empty means all).
        const std::vector<std::string>& vars_to_save() const {
            return vars_to_save_;
        }

        /// Return the behavioral flags for this command.
        const Flags& flags() const { return flags_; }

        /// Dispatch to `CommandVisitor::visit(const EnvCommand&, DiagnosticSink&)`.
        void         accept(CommandVisitor& visitor,
                            DiagnosticSink& sink) const override {
            visitor.visit(*this, sink);
        }
    };

} // namespace math_solver
