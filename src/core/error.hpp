#pragma once

#include "result.hpp"
#include "span.hpp"
#include "ui/color.hpp"
#include <stdexcept>
#include <string>
#include <vector>

namespace math_solver {

    // Returns the span of the first occurrence of `token` in `raw`.
    // - Assumes both `raw` and `token` are valid UTF-8.
    // - Returns empty span if not found or token is empty.
    // - Used for error reporting to highlight relevant source fragments.
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

    struct DiagnosticBuilder {
        // All fields are public for direct construction and mutation.
        // This struct is intended to be a lightweight, non-owning builder for
        // error diagnostics, similar to rustc's Diagnostic.
        std::string level = "error";
        std::string code  = "";
        std::string message;
        std::string filename = "<repl>";
        Span        span;
        std::string input;
        size_t      file_line    = 1;

        std::string inline_label = "";
        std::string help         = "";
        std::string note         = "";

        // Formats the diagnostic for terminal display.
        // - Handles missing input gracefully (no code context).
        // - Computes line/column for span; assumes span is valid in input.
        // - Always underlines at least one character for zero-length spans.
        // - Output is not intended to be machine-readable.
        // - Performance: Acceptable for small/medium inputs; not optimized for
        // large files.
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
            // This is O(N) in the number of bytes before the span, but
            // acceptable for REPL and small files.
            size_t line       = file_line;
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

            // Always underline at least one character, even for zero-length
            // spans.
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

    // Internal exception representing a mathematical error.
    // - Used structurally within deep ast visitors (e.g. evaluator, solver)
    //   and caught at subsystem boundaries to map to Result::err.
    class MathException : public std::runtime_error {
        private:
        Error error_;

        public:
        explicit MathException(const Error& error)
            : std::runtime_error(error.message), error_(error) {}

        const Error&         error() const { return error_; }

        // Fluent API for augmenting diagnostics after construction (for
        // backwards compat at catch sites)
        const MathException& with_code(const std::string& code) {
            error_.code = code;
            return *this;
        }
        const MathException& with_label(const std::string& label) {
            error_.inline_label = label;
            return *this;
        }
        const MathException& with_help(const std::string& help) {
            error_.help = help;
            return *this;
        }
        const MathException& with_note(const std::string& note) {
            error_.note = note;
            return *this;
        }
    };

    // Factory functions for predefined solver errors.
    // Used by internal components to build `Error` instances efficiently.
    namespace errors {

        inline Error math(const std::string& message, const Span& span = Span(),
                          const std::string& input = "") {
            return Error::make(message, "E0000", span, input);
        }

        inline Error parse(const std::string& message,
                           const Span&        span  = Span(),
                           const std::string& input = "") {
            return Error::make(message, "E0001", span, input,
                               "unexpected syntax");
        }

        inline Error unknown_command(const std::string& command,
                                     const Span&        span  = Span(),
                                     const std::string& input = "") {
            auto err = Error::make("unknown command: " + command, "E0002", span,
                                   input, "unrecognized command");
            err.help = "available commands: :help, :env, :config, :history, "
                       ":var, etc.";
            return err;
        }

        inline Error undefined_variable(const std::string& var_name,
                                        const Span&        span  = Span(),
                                        const std::string& input = "") {
            return Error::make("cannot find value `" + var_name +
                                   "` in this scope",
                               "E0425", span, input, "not found in this scope");
        }

        inline Error non_linear(const std::string& message,
                                const Span&        span  = Span(),
                                const std::string& input = "") {
            auto err = Error::make(message, "E0308", span, input,
                                   "expected linear term");
            err.help = "the solver currently only supports linear equations. "
                       "Try defining it as a constant first.";
            return err;
        }

        inline Error multiple_unknowns(const std::vector<std::string>& unknowns,
                                       const Span&        span  = Span(),
                                       const std::string& input = "") {
            std::string msg =
                "cannot solve for multiple variables simultaneously (";
            for (size_t i = 0; i < unknowns.size(); ++i) {
                if (i > 0)
                    msg += ", ";
                msg += "`" + unknowns[i] + "`";
            }
            msg += ")";
            auto err = Error::make(msg, "E0282", span, input,
                                   "multiple unknowns present here");
            err.help =
                "use `:set <var> <expr>` to define the other variables first.";
            return err;
        }

        inline Error circular_dependency(const std::string& var_name,
                                         const Span&        span  = Span(),
                                         const std::string& input = "") {
            auto err = Error::make(
                "cyclic dependency detected for `" + var_name + "`", "E0391",
                span, input, "recursive variable reference");
            err.help = "ensure that variables do not depend on themselves "
                       "directly or indirectly.";
            return err;
        }

        inline Error
        no_solution(const std::string& message = "equation has no solution",
                    const Span& span = Span(), const std::string& input = "") {
            return Error::make(message, "E0010", span, input,
                               "unsatisfiable equation");
        }

        inline Error infinite_solutions(
            const std::string& message = "equation has infinite solutions",
            const Span& span = Span(), const std::string& input = "") {
            return Error::make(message, "E0011", span, input,
                               "tautology detected");
        }

        inline Error invalid_equation(const std::string& message,
                                      const Span&        span  = Span(),
                                      const std::string& input = "") {
            return Error::make(message, "E0012", span, input,
                               "invalid equation format");
        }

        inline Error reserved_keyword(const std::string& keyword,
                                      const Span&        span  = Span(),
                                      const std::string& input = "") {
            auto err =
                Error::make("`" + keyword + "` is a reserved keyword", "E0013",
                            span, input, "reserved keyword used as identifier");
            err.help = "choose a different name for your variable.";
            return err;
        }

        inline Error polynomial(const std::string& message,
                                const Span&        span  = Span(),
                                const std::string& input = "") {
            return Error::make(message, "E0500", span, input,
                               "polynomial error");
        }

    } // namespace errors

} // namespace math_solver