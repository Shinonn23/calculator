#pragma once

//! # Module — `src/ast/command/math_command.hpp`
//!
//! Defines `MathCommand`, the AST node for math-operation commands such as
//! evaluate, solve, simplify, expand, and factor. Part of the command layer;
//! produced by the command parser and dispatched via `CommandRegistry`.

#include <string>
#include <vector>

#include "command.hpp"
#include "command_visitor.hpp"

namespace math_solver {
    /// AST node for a user-requested math operation.
    ///
    /// Encodes the operation kind (`Type`), the expression payload string, and
    /// optional modifier flags. The raw input string is preserved in the base
    /// `Command` for diagnostics and round-tripping.
    ///
    /// Invariant: `specific_vars_` is empty unless the operation type supports
    /// variable targeting. `isolated_` and `as_fraction_` are orthogonal; both
    /// may be set simultaneously.
    class MathCommand : public Command {
        public:
        /// The high-level math operation requested by the user.
        ///
        /// `Unknown` is used for unrecognized operation names to avoid hard
        /// failures during parsing.
        enum class Type { Evaluate, Solve, Simplify, Expand, Factor, Unknown };

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
        /// Construct a `MathCommand` with a given operation type and payload.
        ///
        /// # Arguments
        ///
        /// * `type`    — The requested math operation.
        /// * `payload` — The raw expression string to operate on.
        /// * `raw`     — The full raw input line for diagnostics.
        MathCommand(Type type, const std::string& payload,
                    const std::string& raw)
            : Command(raw), type_(type), payload_(payload) {}

        /// Set optional modifier flags for this command.
        ///
        /// Must be called before the command is dispatched to a handler.
        /// Default flag values correspond to no modifiers being present.
        ///
        /// # Arguments
        ///
        /// * `isolated`  — Evaluate without referencing context variables.
        /// * `fraction`  — Render numeric results as fractions when possible.
        /// * `vars`      — Restrict the operation to these variable names only.
        void set_flags(bool isolated, bool fraction,
                       const std::vector<std::string>& vars = {}) {
            isolated_      = isolated;
            as_fraction_   = fraction;
            specific_vars_ = vars;
        }

        /// Return the operation type.
        Type               type() const { return type_; }

        /// Return the raw expression payload string.
        const std::string& payload() const { return payload_; }

        /// Return true if the isolated flag is set.
        bool               isolated() const { return isolated_; }

        /// Return true if the as-fraction flag is set.
        bool               as_fraction() const { return as_fraction_; }

        /// Return the list of targeted variable names, or an empty vector if none.
        const std::vector<std::string>& specific_vars() const {
            return specific_vars_;
        }

        /// Dispatch to `CommandVisitor::visit(const MathCommand&, DiagnosticSink&)`.
        void accept(CommandVisitor& visitor,
                    DiagnosticSink& sink) const override {
            visitor.visit(*this, sink);
        }
    };
} // namespace math_solver
