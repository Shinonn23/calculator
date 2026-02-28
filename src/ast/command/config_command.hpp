#pragma once

//! # Module — `src/ast/command/config_command.hpp`
//!
//! Defines `ConfigCommand`, the AST node for configuration commands
//! (`:config list`, `:config get <key>`, `:config set <key> <value>`, etc.).
//! Part of the command layer; produced by the config subparser and dispatched
//! via `CommandRegistry`.

#include "command.hpp"
#include "command_visitor.hpp"

namespace math_solver {
    /// AST node for a configuration management command.
    ///
    /// Encodes the requested config operation (`Action`) and an optional
    /// key/value pair for `Get` and `Set` actions. `key_` and `value_` are
    /// only meaningful for `Get`/`Set`; for other actions they may be empty.
    ///
    /// Invariants:
    /// * `action_` is set at construction and never mutated.
    /// * `key_` and `value_` must be populated via `set_kv` before dispatching
    ///   a `Get` or `Set` command.
    class ConfigCommand : public Command {
        public:
        /// The configuration operation to perform.
        enum class Action { List, Get, Set, Path, Reset, Unknown };

        private:
        Action      action_;
        std::string key_;
        std::string value_;

        public:
        /// Construct a `ConfigCommand` for the given action.
        ///
        /// # Arguments
        ///
        /// * `act` — The config operation.
        /// * `raw` — The full raw input line for diagnostics.
        ConfigCommand(Action act, const std::string& raw)
            : Command(raw), action_(act) {}

        /// Set the key/value pair for `Get` or `Set` actions.
        ///
        /// Must be called before the command is dispatched. Irrelevant for
        /// actions other than `Get` and `Set`.
        ///
        /// # Arguments
        ///
        /// * `key`   — The configuration key name.
        /// * `value` — The new value (empty for `Get` actions).
        void set_kv(const std::string& key, const std::string& value = "") {
            key_   = key;
            value_ = value;
        }

        /// Return the configuration operation kind.
        Action             action() const { return action_; }

        /// Return the configuration key name (meaningful for `Get`/`Set` only).
        const std::string& key() const { return key_; }

        /// Return the configuration value (meaningful for `Set` only).
        const std::string& value() const { return value_; }

        /// Dispatch to `CommandVisitor::visit(const ConfigCommand&, DiagnosticSink&)`.
        void               accept(CommandVisitor& visitor,
                                  DiagnosticSink& sink) const override {
            visitor.visit(*this, sink);
        }
    };
} // namespace math_solver
