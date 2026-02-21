#pragma once

#include "span.hpp"
#include "ui/color.hpp"
#include <stdexcept>
#include <string>
#include <vector>

namespace math_solver {

    // Returns the span of the first occurrence of `token` in `raw`.
    // Returns an empty span if not found or if token is empty.
    // Assumes `raw` and `token` are valid UTF-8.
    inline Span find_token_span(const std::string& raw,
                                const std::string& token) {
        if (token.empty())
            return Span();
        size_t pos = raw.find(token);
        if (pos != std::string::npos) {
            return Span(pos, pos + token.length());
        }
        return Span();
    }

    // DiagnosticBuilder is a utility for constructing error diagnostics
    // with rich context, modeled after rustc's error reporting.
    // - All fields are public for direct construction.
    // - Formatting logic is centralized in build().
    // - Assumes input is a single source file or REPL input.
    struct DiagnosticBuilder {
        std::string level = "error";
        std::string code  = "";
        std::string message;
        std::string filename = "<repl>";
        Span        span;
        std::string input;

        std::string inline_label = "";
        std::string help         = "";
        std::string note         = "";

        // Formats the diagnostic as a colored, multi-line string.
        // - Handles missing input gracefully (no code context).
        // - Computes line/column for span; assumes span is valid in input.
        // - Underlines at least one character for zero-length spans.
        // - Output is intended for terminal display; not machine-readable.
        std::string build() const {
            std::string out;

            out += ansi::red;
            out += ansi::bold;
            out += level;
            if (!code.empty())
                out += "[" + code + "]";
            out += ansi::reset;
            out += ansi::bold;
            out += ": " + message;
            out += ansi::reset;
            out += "\n";

            if (input.empty()) {
                if (!help.empty())
                    out += "  = help: " + help + "\n";
                if (!note.empty())
                    out += "  = note: " + note + "\n";
                return out;
            }

            // Compute line/column for the start of the span.
            // This logic is performance-sensitive for large inputs,
            // but is acceptable for typical REPL or small file usage.
            size_t line       = 1;
            size_t line_start = 0;
            for (size_t i = 0; i < span.start && i < input.size(); ++i) {
                if (input[i] == '\n') {
                    line++;
                    line_start = i + 1;
                }
            }
            size_t col =
                (span.start >= line_start) ? (span.start - line_start + 1) : 1;

            size_t line_end = input.find('\n', line_start);
            if (line_end == std::string::npos)
                line_end = input.size();
            std::string source_line =
                input.substr(line_start, line_end - line_start);

            std::string line_str = std::to_string(line);
            std::string margin_pad(line_str.size(), ' ');

            out += ansi::cyan;
            out += " " + margin_pad + "--> ";
            out += ansi::reset;
            out += filename + ":" + std::to_string(line) + ":" +
                   std::to_string(col) + "\n";

            out += ansi::cyan;
            out += " " + margin_pad + " |";
            out += ansi::reset;
            out += "\n";

            out += ansi::cyan;
            out += " " + line_str + " | ";
            out += ansi::reset;
            out += source_line + "\n";

            out += ansi::cyan;
            out += " " + margin_pad + " | ";
            out += ansi::reset;

            // Underline the span; always at least one caret.
            for (size_t i = 0; i < col - 1; ++i)
                out += ' ';

            size_t len = span.length();
            if (len == 0)
                len = 1;
            out += ansi::red;
            out += ansi::bold;
            for (size_t i = 0; i < len; ++i)
                out += '^';
            if (!inline_label.empty()) {
                out += " " + inline_label;
            }
            out += ansi::reset;
            out += "\n";

            if (!help.empty()) {
                out += ansi::cyan;
                out += " " + margin_pad + " =";
                out += ansi::reset;
                out += " ";
                out += ansi::bold;
                out += "help:";
                out += ansi::reset;
                out += " " + help + "\n";
            }
            if (!note.empty()) {
                out += ansi::cyan;
                out += " " + margin_pad + " =";
                out += ansi::reset;
                out += " ";
                out += ansi::bold;
                out += "note:";
                out += ansi::reset;
                out += " " + note + "\n";
            }

            return out;
        }
    };

    // MathError is the base class for all solver errors.
    // - Carries a DiagnosticBuilder for rich error reporting.
    // - diag_ is mutable to allow post-construction augmentation (e.g.
    // help/note)
    //   during error handling, similar to how rustc augments diagnostics in
    //   passes.
    class MathError : public std::runtime_error {
        protected:
        mutable DiagnosticBuilder diag_;

        public:
        MathError(const std::string& message,
                  const Span&        span  = Span(),
                  const std::string& input = "")
            : std::runtime_error(message) {
            diag_.message = message;
            diag_.span    = span;
            diag_.input   = input;
        }

        const Span&        span() const { return diag_.span; }
        const std::string& input() const { return diag_.input; }

        void set_input(const std::string& input) const { diag_.input = input; }
        void set_filename(const std::string& filename) const {
            diag_.filename = filename;
        }

        // Fluent API for augmenting diagnostics after construction.
        // - Returns *this for chaining.
        // - Used to attach codes, labels, help, and notes at the catch site.
        const MathError& with_code(const std::string& code) const {
            diag_.code = code;
            return *this;
        }
        const MathError& with_label(const std::string& label) const {
            diag_.inline_label = label;
            return *this;
        }
        const MathError& with_help(const std::string& help) const {
            diag_.help = help;
            return *this;
        }
        const MathError& with_note(const std::string& note) const {
            diag_.note = note;
            return *this;
        }

        std::string format() const { return diag_.build(); }
    };

    // --- Specialized error types for solver diagnostics.
    // Each subclass sets a unique error code and label.
    // Codes are chosen to match rustc conventions where applicable.

    class ParseError : public MathError {
        public:
        ParseError(const std::string& message,
                   const Span&        span  = Span(),
                   const std::string& input = "")
            : MathError(message, span, input) {
            diag_.code         = "E0001";
            diag_.inline_label = "unexpected syntax";
        }
    };

    class UnknownCommandError : public MathError {
        public:
        UnknownCommandError(const std::string& command,
                            const Span&        span  = Span(),
                            const std::string& input = "")
            : MathError("unknown command: " + command, span, input) {
            diag_.code         = "E0002";
            diag_.inline_label = "unrecognized command";
            diag_.help = "available commands: :help, :env, :config, :history, "
                         ":var, etc.";
        }
    };

    class UndefinedVariableError : public MathError {
        private:
        std::string var_name_;

        public:
        UndefinedVariableError(const std::string& var_name,
                               const Span&        span  = Span(),
                               const std::string& input = "")
            : MathError("cannot find value `" + var_name + "` in this scope",
                        span,
                        input),
              var_name_(var_name) {
            diag_.code         = "E0425";
            diag_.inline_label = "not found in this scope";
        }

        const std::string& var_name() const { return var_name_; }
    };

    class NonLinearError : public MathError {
        public:
        NonLinearError(const std::string& message,
                       const Span&        span  = Span(),
                       const std::string& input = "")
            : MathError(message, span, input) {
            diag_.code         = "E0308";
            diag_.inline_label = "expected linear term";
            diag_.help = "the solver currently only supports linear equations. "
                         "Try defining it as a constant first.";
        }
    };

    class MultipleUnknownsError : public MathError {
        private:
        std::vector<std::string> unknowns_;

        public:
        MultipleUnknownsError(const std::vector<std::string>& unknowns,
                              const Span&                     span  = Span(),
                              const std::string&              input = "")
            : MathError(build_message(unknowns), span, input),
              unknowns_(unknowns) {
            diag_.code         = "E0282";
            diag_.inline_label = "multiple unknowns present here";
            diag_.help =
                "use `:set <var> <expr>` to define the other variables first.";
        }
        const std::vector<std::string>& unknowns() const { return unknowns_; }

        private:
        // Constructs a message listing all unknowns.
        static std::string build_message(const std::vector<std::string>& vars) {
            std::string msg =
                "cannot solve for multiple variables simultaneously (";
            for (size_t i = 0; i < vars.size(); ++i) {
                if (i > 0)
                    msg += ", ";
                msg += "`" + vars[i] + "`";
            }
            msg += ")";
            return msg;
        }
    };

    class NoSolutionError : public MathError {
        public:
        NoSolutionError(const std::string& message = "equation has no solution",
                        const Span&        span    = Span(),
                        const std::string& input   = "")
            : MathError(message, span, input) {
            diag_.code         = "E0010";
            diag_.inline_label = "unsatisfiable equation";
        }
    };

    class InfiniteSolutionsError : public MathError {
        public:
        InfiniteSolutionsError(
            const std::string& message = "equation has infinite solutions",
            const Span&        span    = Span(),
            const std::string& input   = "")
            : MathError(message, span, input) {
            diag_.code         = "E0011";
            diag_.inline_label = "tautology detected";
        }
    };

    class InvalidEquationError : public MathError {
        public:
        InvalidEquationError(const std::string& message,
                             const Span&        span  = Span(),
                             const std::string& input = "")
            : MathError(message, span, input) {
            diag_.code         = "E0012";
            diag_.inline_label = "invalid equation format";
        }
    };

    class ReservedKeywordError : public MathError {
        public:
        ReservedKeywordError(const std::string& keyword,
                             const Span&        span  = Span(),
                             const std::string& input = "")
            : MathError(
                  "`" + keyword + "` is a reserved keyword", span, input) {
            diag_.code         = "E0013";
            diag_.inline_label = "reserved keyword used as identifier";
            diag_.help         = "choose a different name for your variable.";
        }
    };

    class CircularDependencyError : public MathError {
        private:
        std::string var_name_;

        public:
        CircularDependencyError(const std::string& var_name,
                                const Span&        span  = Span(),
                                const std::string& input = "")
            : MathError("cyclic dependency detected for `" + var_name + "`",
                        span,
                        input),
              var_name_(var_name) {
            diag_.code         = "E0391";
            diag_.inline_label = "recursive variable reference";
            diag_.help = "ensure that variables do not depend on themselves "
                         "directly or indirectly.";
        }
        const std::string& var_name() const { return var_name_; }
    };

} // namespace math_solver