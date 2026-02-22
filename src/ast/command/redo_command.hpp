#pragma once

#include "command.hpp"
#include <vector>

namespace math_solver {
    class RedoCommand : public Command {
        // Invariant: range_ is either empty (indicating a redo of the last
        // command) or contains indices specifying a contiguous range of
        // commands to redo. The interpretation of these indices is expected to
        // be consistent with the undo/redo stack semantics elsewhere in the
        // system.
        std::vector<int> range_;

        public:
        RedoCommand(const std::string& raw) : Command(raw) {}

        void set_range(const std::vector<int>& range) { range_ = range; }
        const std::vector<int>& range() const { return range_; }

        // Accepts a visitor for double-dispatch; relies on CommandVisitor
        // implementations to correctly handle the redo semantics.
        // Correctness depends on the visitor not mutating this object.
        void                    accept(CommandVisitor& visitor,
                                       DiagnosticSink& sink) const override {
            visitor.visit(*this, sink);
        }
    };
} // namespace math_solver
