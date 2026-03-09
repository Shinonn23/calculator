#pragma once

//! # Module — `src/ast/command/var_command.hpp`
//!
//! Defines `VarCommand`, the AST node for variable-management commands
//! (`:set`, `:unset`). Optionally carries a math action (e.g., `"solve"`,
//! `"expand"`) that instructs downstream handlers to apply an algebra pass
//! before assigning the result to the variable.

#include <cassert>
#include <optional>
#include <string>
#include <vector>

#include "command.hpp"
#include "command_visitor.hpp"
#include "core/span.hpp"

namespace math_solver {
    /// AST node for a variable-management command.
    ///
    /// Represents either a variable assignment (`Action::Set`) or removal
    /// (`Action::Unset`). For `Set`, an optional `math_action_` field names a
    /// higher-level algebra operation (e.g., `"solve"`, `"expand"`) to apply
    /// to the payload expression before storing the result.
    ///
    /// Invariants:
    /// * `var_name_` must be a valid identifier.
    /// * For `Action::Unset`, `payload_` and `math_action_` must be absent.
    /// * `math_action()` must only be called after a `has_math_action()` guard;
    ///   calling it without the guard aborts via `assert`.
    class VarCommand : public Command {
        public:
        /// The variable operation to perform.
        enum class Action { Set, Unset, Unknown };

        private:
        Action                     action_;
        std::vector<std::string>   var_name_;
        std::optional<std::string> payload_;
        std::optional<std::string> math_action_;
        Span                       var_name_span_;

        public:
        /// Construct a `VarCommand` for the given action and variable name.
        ///
        /// # Arguments
        ///
        /// * `act` — The variable operation (`Set`, `Unset`, or `Unknown`).
        /// * `var` — The target variable identifier.
        /// * `raw` — The full raw input line for diagnostics.
        VarCommand(Action act, const std::vector<std::string>& var,
                   const std::string& raw)
            : Command(raw), action_(act), var_name_(var) {}

        /// Set the math action and its associated expression payload.
        ///
        /// Caller must ensure semantic consistency between `action` and
        /// `payload`; no validation is performed here.
        ///
        /// # Arguments
        ///
        /// * `action`  — Name of the algebra operation (e.g., `"solve"`).
        /// * `payload` — The expression string to pass to that operation.
        void set_payload(const std::string& action,
                         const std::string& payload) {
            math_action_ = action;
            payload_     = payload;
        }

        /// Return the variable operation kind.
        Action                          action() const { return action_; }

        /// Set the span of the first var-name token recorded at parse time.
        void set_var_name_span(const Span& s) { var_name_span_ = s; }

        /// Return the span of the first var-name token (empty if not set).
        const Span& var_name_span() const { return var_name_span_; }

        /// Return the target variable name.
        const std::vector<std::string>& var_name() const { return var_name_; }

        /// Return true if an expression payload is present.
        bool               has_payload() const { return payload_.has_value(); }

        /// Return the expression payload string.
        ///
        /// # Panics
        ///
        /// Undefined behaviour if called when `has_payload()` is false.
        const std::string& payload() const { return *payload_; }

        /// Return the math action name.
        ///
        /// # Panics
        ///
        /// Aborts via `assert` if `has_math_action()` is false.
        const std::string& math_action() const {
            assert(math_action_.has_value() &&
                   "math_action() called when math_action_ is not set");
            return *math_action_;
        }

        /// Return true if a math action has been set via `set_payload`.
        bool has_math_action() const { return math_action_.has_value(); }

        /// Dispatch to `CommandVisitor::visit(const VarCommand&,
        /// DiagnosticSink&)`.
        void accept(CommandVisitor& visitor,
                    DiagnosticSink& sink) const override {
            visitor.visit(*this, sink);
        }
    };

} // namespace math_solver
