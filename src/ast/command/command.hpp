#ifndef COMMAND
#define COMMAND

#include <memory>
#include <string>

#include "command_visitor.hpp"

namespace math_solver {

    // Base class for all command AST nodes.
    //
    // - Each Command instance owns its raw textual representation, which is
    //   preserved for diagnostics and round-tripping.
    // - Subclasses must implement accept() for the visitor pattern; this is
    //   used by all passes that operate on commands (e.g., semantic analysis,
    //   lowering, etc).
    // - Lifetime: Command objects are always heap-allocated and managed via
    //   CommandPtr. No shared ownership is assumed.
    // - Invariant: raw_command_ must always be a valid, non-empty string
    //   corresponding to the original user input.
    class Command {
        protected:
        std::string raw_command_;

        public:
        explicit Command(const std::string& raw) : raw_command_(raw) {}
        virtual ~Command() = default;

        // Returns the original command string as provided by the user.
        // Used for error reporting and debugging; not intended for parsing.
        const std::string& raw_command() const { return raw_command_; }

        // Accepts a visitor for double-dispatch. Subclasses must implement
        // this to ensure correct dispatch for all command kinds.
        virtual void       accept(CommandVisitor& visitor) const = 0;
    };

    // Unique ownership for Command AST nodes.
    using CommandPtr = std::unique_ptr<Command>;

} // namespace math_solver

#endif