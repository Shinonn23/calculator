#pragma once

#include "command.hpp"
#include <string>

namespace math_solver {

    // Represents a command to load external resources (e.g., files,
    // environments) into the solver context. The Flags struct encodes
    // behavioral modifiers that affect how the load is performed. Notably:
    // - dry_run: If true, the load is simulated but not executed. Used by
    //   higher-level passes to validate command sequences without side effects.
    // - silent: Suppresses user-facing output; intended for batch or scripted
    //   invocations where noise must be minimized.
    // - env: Optionally specifies an environment context for the load, which
    //   may affect downstream resolution or scoping.
    //
    // Invariants:
    // - filepath_ must be a valid, non-empty path when the command is executed.
    // - Flags are assumed to be set by the parser; no runtime validation here.
    //
    // Interacts with: CommandVisitor, which is responsible for dispatching
    // execution logic. Any changes to the visitor protocol must account for
    // LoadCommand's semantics.
    class LoadCommand : public Command {
        public:
        struct Flags {
            bool        dry_run = false;
            bool        silent  = false;
            std::string env;
        };

        LoadCommand(const std::string& filepath, const Flags& flags,
                    const std::string& raw)
            : Command(raw), filepath_(filepath), flags_(flags) {}

        const std::string& filepath() const { return filepath_; }
        const Flags&       flags() const { return flags_; }

        void               accept(CommandVisitor& visitor,
                                  DiagnosticSink& sink) const override {
            visitor.visit(*this, sink);
        }

        private:
        std::string filepath_;
        Flags       flags_;
    };

} // namespace math_solver