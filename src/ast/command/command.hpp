#pragma once

#include <memory>
#include <string>

#include "command_visitor.hpp"

namespace math_solver {

    class DiagnosticSink;

    class Command {
        protected:
        std::string raw_command_;
        // Tracks the origin of the command for diagnostics and error reporting.
        // Invariants:
        // - source_file_ is always non-empty.
        // - source_line_ is always >= 1.
        std::string source_file_ = "<repl>";
        size_t      source_line_ = 1;

        public:
        explicit Command(const std::string& raw) : raw_command_(raw) {}
        virtual ~Command() = default;

        const std::string& raw_command() const { return raw_command_; }
        const std::string& source_file() const { return source_file_; }
        size_t             source_line() const { return source_line_; }

        // Updates the source location metadata.
        // Used by the parser to associate commands with their origin.
        // Must be called before any error reporting that depends on source
        // location.
        void               set_source(const std::string& file, size_t line) {
            source_file_ = file;
            source_line_ = line;
        }

        // Accepts a visitor for double-dispatch.
        // Subclasses must implement this to participate in the visitor pattern.
        virtual void accept(CommandVisitor& visitor,
                            DiagnosticSink& sink) const = 0;
    };

    using CommandPtr = std::unique_ptr<Command>;

} // namespace math_solver