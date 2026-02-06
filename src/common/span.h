#ifndef SPAN_H
#define SPAN_H

#include <cstddef>
#include <string>

using namespace std;

namespace math_solver {
    struct Span {
        size_t start;
        size_t end;

        Span() : start(0), end(0) {}
        Span(size_t s, size_t e) : start(s), end(e) {}

        // Merge two spans to cover both
        Span merge(const Span& other) const {
            return Span(
                start < other.start ? start : other.start,
                end > other.end ? end : other.end
            );
        }

        size_t length() const { return end - start; }

        bool empty() const { return start == end; }
    };

    // Format error with source line and caret pointer
    inline string format_error_at_span(
        const string& message,
        const string& input,
        const Span& span
    ) {
        string result = "Error: " + message + "\n";
        result += "  " + input + "\n";
        result += "  ";

        // Add spaces up to span start
        for (size_t i = 0; i < span.start && i < input.size(); ++i) {
            result += ' ';
        }

        // Add carets for span length
        size_t len = span.length();
        if (len == 0) len = 1;
        for (size_t i = 0; i < len && (span.start + i) < input.size(); ++i) {
            result += '^';
        }
        if (span.start >= input.size()) {
            result += '^';
        }

        return result;
    }

    // Format warning with source line
    inline string format_warning_at_span(
        const string& message,
        const string& input,
        const Span& span
    ) {
        string result = "Warning: " + message + "\n";
        result += "  " + input + "\n";
        result += "  ";

        for (size_t i = 0; i < span.start && i < input.size(); ++i) {
            result += ' ';
        }

        size_t len = span.length();
        if (len == 0) len = 1;
        for (size_t i = 0; i < len && (span.start + i) < input.size(); ++i) {
            result += '~';
        }

        return result;
    }

} // namespace math_solver

#endif
