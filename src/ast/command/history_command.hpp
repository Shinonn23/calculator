#pragma once

//! # Module — `src/ast/command/history_command.hpp`
//!
//! Defines `HistoryCommand`, the AST node for history-related commands
//! (show, show-range, search, save, clear). Part of the command layer; produced
//! by the history subparser and dispatched via `CommandRegistry`.

#include <string>
#include <vector>

#include "command.hpp"
#include "command_visitor.hpp"

namespace math_solver {

    /// AST node for a history management command.
    ///
    /// The meaningful fields depend on `action_`:
    /// * `Show`      — `limit_` bounds the number of entries (0 means all).
    /// * `ShowRange` — `range_` is non-empty, sorted, unique, and 1-based.
    /// * `Search`    — `pattern_` is non-empty.
    /// * `Save`      — `filepath_` is non-empty; `range_` is optional.
    /// * `Clear`     — No additional fields are required.
    ///
    /// Flag filtering applies to `Show`, `ShowRange`, and `Search` actions.
    /// Enforcing per-action invariants is the responsibility of the parser.
    class HistoryCommand : public Command {
        public:
        /// The history operation to perform.
        enum class Action { Show, ShowRange, Search, Save, Clear, Unknown };

        private:
        Action           action_;
        int              limit_ = 20;
        std::string      pattern_;
        std::string      filepath_;
        std::vector<int> range_;

        public:
        /// Status filter applied when listing history entries.
        enum class Flag { None, Errors, Success, Warning, Info };
        std::vector<Flag> flags_;

        /// Set the active status filters, discarding unrecognized values.
        ///
        /// Only `Errors`, `Success`, `Warning`, and `Info` are valid; any
        /// other `Flag` value is mapped to `Flag::None`.
        ///
        /// # Arguments
        ///
        /// * `flags` — The set of status filters to apply.
        void              set_flags(const std::vector<Flag>& flags) {
            std::vector<Flag> new_flags;
            for (const auto& flag : flags) {
                if (flag == HistoryCommand::Flag::Errors)
                    new_flags.push_back(Flag::Errors);
                else if (flag == HistoryCommand::Flag::Success)
                    new_flags.push_back(Flag::Success);
                else if (flag == HistoryCommand::Flag::Warning)
                    new_flags.push_back(Flag::Warning);
                else if (flag == HistoryCommand::Flag::Info)
                    new_flags.push_back(Flag::Info);
                else
                    new_flags.push_back(Flag::None);
            }
            flags_ = new_flags;
        }

        /// Return true if the given status flag is present in the active filter set.
        bool has_flag(Flag flag) const {
            for (const auto& f : flags_) {
                if (f == flag)
                    return true;
            }
            return false;
        }

        /// Return true if any status filter is active.
        bool has_any_flag() const { return !flags_.empty(); }

        /// Construct a `HistoryCommand` for the given action and raw input.
        HistoryCommand(Action action, const std::string& raw)
            : Command(raw), action_(action) {}

        /// Return the active status filters.
        const std::vector<Flag>& flags() const { return flags_; }

        /// Set the maximum number of entries to display (0 means all).
        void                     set_limit(int limit) { limit_ = limit; }

        /// Set the search pattern for `Search` actions.
        void   set_pattern(const std::string& pattern) { pattern_ = pattern; }

        /// Set the output file path for `Save` actions.
        void   set_filepath(const std::string& path) { filepath_ = path; }

        /// Set the 1-based entry index range for `ShowRange` actions.
        ///
        /// # Arguments
        ///
        /// * `range` — Sorted, unique, 1-based list of entry indices.
        void   set_range(const std::vector<int>& range) { range_ = range; }

        /// Return the history operation kind.
        Action action() const { return action_; }

        /// Return the display limit (default 20; 0 means all).
        int    limit() const { return limit_; }

        /// Return the search pattern string.
        const std::string&      pattern() const { return pattern_; }

        /// Return the output file path.
        const std::string&      filepath() const { return filepath_; }

        /// Return the entry index range.
        const std::vector<int>& range() const { return range_; }

        /// Return true if this action only reads history without mutating it.
        ///
        /// `Show`, `ShowRange`, and `Search` are considered read-only.
        bool                    is_readonly() const {
            return action_ == Action::Show || action_ == Action::ShowRange ||
                   action_ == Action::Search;
        }

        /// Dispatch to `CommandVisitor::visit(const HistoryCommand&, DiagnosticSink&)`.
        void accept(CommandVisitor& visitor,
                    DiagnosticSink& sink) const override {
            visitor.visit(*this, sink);
        }
    };

} // namespace math_solver
