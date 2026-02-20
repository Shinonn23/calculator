#ifndef SPAN_H
#define SPAN_H

#include <cstddef>
#include <string>

namespace math_solver {

    struct Span {
        size_t start;
        size_t end;

        // Invariant: [start, end) is a half-open interval into some source
        // buffer. end >= start is assumed by all consumers.
        Span() : start(0), end(0) {}
        Span(size_t s, size_t e) : start(s), end(e) {}

        // Returns the minimal Span covering both this and `other`.
        // Used to propagate error/warning ranges through transformations.
        // Assumes both spans refer to the same underlying buffer.
        Span merge(const Span& other) const {
            return Span(start < other.start ? start : other.start,
                        end > other.end ? end : other.end);
        }

        // Returns the number of bytes/chars covered by this span.
        // No bounds checking; caller must ensure end >= start.
        size_t length() const { return end - start; }

        // Returns true if the span is empty (start == end).
        // Used to indicate zero-width locations (e.g., point errors).
        bool   empty() const { return start == end; }
    };

    // Formats an error message with a source line and caret(s) indicating the
    // span.
    // - If span is empty, emits a single caret at start.
    // - If span extends past input, carets are capped at input length and a
    // single caret is appended.
    // - Used for diagnostics; not performance-critical.
    inline std::string format_error_at_span(const std::string& message,
                                            const std::string& input,
                                            const Span&        span) {
        std::string result = "Error: " + message + "\n";
        result += "  " + input + "\n";
        result += "  ";

        for (size_t i = 0; i < span.start && i < input.size(); ++i) {
            result += ' ';
        }

        size_t len = span.length();
        if (len == 0)
            len = 1;
        for (size_t i = 0; i < len && (span.start + i) < input.size(); ++i) {
            result += '^';
        }
        if (span.start >= input.size()) {
            result += '^'; // Handles spans that point past end of input (e.g.,
                           // EOF errors).
        }

        return result;
    }

    // Formats a warning message with a source line and tildes indicating the
    // span.
    // - Used for non-fatal diagnostics.
    // - Follows same edge case handling as error formatting.
    inline std::string format_warning_at_span(const std::string& message,
                                              const std::string& input,
                                              const Span&        span) {
        std::string result = "Warning: " + message + "\n";
        result += "  " + input + "\n";
        result += "  ";

        for (size_t i = 0; i < span.start && i < input.size(); ++i) {
            result += ' ';
        }

        size_t len = span.length();
        if (len == 0)
            len = 1;
        for (size_t i = 0; i < len && (span.start + i) < input.size(); ++i) {
            result += '~';
        }

        return result;
    }

} // namespace math_solver

#endif
