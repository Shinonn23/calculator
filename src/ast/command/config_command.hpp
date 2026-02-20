#ifndef CONFIG_COMMAND
#define CONFIG_COMMAND

#include "command.hpp"
#include "command_visitor.hpp"

namespace math_solver {
    // Represents a configuration command in the AST.
    //
    // Invariants:
    // - `action_` is always set at construction and never mutated.
    // - `key_` and `value_` are only meaningful for Get/Set actions; for
    // others, they may be empty.
    //
    // Notes:
    // - The enum Action must remain in sync with the parser logic; changes here
    // may require updates elsewhere.
    // - Accepts a CommandVisitor for double-dispatch; relies on visitor to
    // handle all Action variants.
    // - `raw` is passed to the base Command for error reporting and
    // diagnostics.
    class ConfigCommand : public Command {
        public:
        enum class Action { List, Get, Set, Path, Reset };

        private:
        Action      action_;
        std::string key_;
        std::string value_;

        public:
        ConfigCommand(Action act, const std::string& raw)
            : Command(raw), action_(act) {}

        // Sets the key/value pair for Get/Set actions.
        // Caller must ensure this is only invoked for relevant actions.
        void set_kv(const std::string& key, const std::string& value = "") {
            key_   = key;
            value_ = value;
        }

        Action             action() const { return action_; }
        const std::string& key() const { return key_; }
        const std::string& value() const { return value_; }

        // Double-dispatch entry point; relies on visitor to distinguish Action
        // variants. All visitors must be prepared to handle all Action values.
        void               accept(CommandVisitor& visitor) const override {
            visitor.visit(*this);
        }
    };
} // namespace math_solver

#endif