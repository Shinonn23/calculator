#pragma once

#include <string>
#include <vector>

#include "command.hpp"
#include "command_visitor.hpp"

namespace math_solver {

    // AST node for history-related commands.
    //
    // Invariants:
    // - `action_` determines which fields are relevant; see below.
    // - For Action::Show, `limit_` bounds the number of entries (0 = all).
    // - For Action::ShowRange, `range_` must be non-empty, sorted, unique,
    // 1-based.
    // - For Action::Search, `pattern_` must be non-empty.
    // - For Action::Save, `filepath_` must be non-empty; `range_` optional.
    // - For Action::Clear, no additional fields are required.
    //
    // Correctness relies on external code (parser, command builder) to enforce
    // invariants. Mutations to history are only permitted for non-readonly
    // actions. Flags are used for filtering and may interact with UI or
    // reporting layers.
    class HistoryCommand : public Command {
        public:
        enum class Action { Show, ShowRange, Search, Save, Clear, Unknown };

        private:
        Action           action_;
        int              limit_ = 20;
        std::string      pattern_;
        std::string      filepath_;
        std::vector<int> range_;

        public:
        enum class Flag { None, Errors, Success, Warning, Info };
        std::vector<Flag> flags_;

        // Only Errors and Success are currently supported for filtering.
        // Any other flag is mapped to None to avoid accidental propagation.
        void              set_flags(const std::vector<Flag>& flags) {
            std::vector<Flag> new_flags;
            for (const auto& flag : flags) {
                if (flag == HistoryCommand::Flag::Errors)
                    new_flags.push_back(Flag::Errors);
                else if (flag == HistoryCommand::Flag::Success)
                    new_flags.push_back(Flag::Success);
                else
                    new_flags.push_back(Flag::None);
            }
            flags_ = new_flags;
        }

        bool has_flag(Flag flag) const {
            for (const auto& f : flags_) {
                if (f == flag)
                    return true;
            }
            return false;
        }

        bool has_any_flag() const { return !flags_.empty(); }

        HistoryCommand(Action action, const std::string& raw)
            : Command(raw), action_(action) {}

        const std::vector<Flag>& flags() const { return flags_; }

        void                     set_limit(int limit) { limit_ = limit; }

        void   set_pattern(const std::string& pattern) { pattern_ = pattern; }

        void   set_filepath(const std::string& path) { filepath_ = path; }

        // Caller must ensure range is sorted, unique, and 1-based.
        void   set_range(const std::vector<int>& range) { range_ = range; }

        Action action() const { return action_; }
        int    limit() const { return limit_; }
        const std::string&      pattern() const { return pattern_; }
        const std::string&      filepath() const { return filepath_; }
        const std::vector<int>& range() const { return range_; }

        // Only Show, ShowRange, and Search are considered readonly.
        // Used to avoid polluting history with non-mutating queries.
        bool                    is_readonly() const {
            return action_ == Action::Show || action_ == Action::ShowRange ||
                   action_ == Action::Search;
        }

        void accept(CommandVisitor& visitor,
                    DiagnosticSink& sink) const override {
            visitor.visit(*this, sink);
        }
    };

} // namespace math_solver
