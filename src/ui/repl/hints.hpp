#pragma once

#include <replxx.hxx>
#include <string>

namespace math_solver {

    namespace detail {

        // Associates REPL command keywords with their expected argument syntax.
        // - Used to provide inline hints during command completion.
        // - Assumes keywords are trimmed and match exactly; no fuzzy matching.
        // - If a keyword is not recognized, returns the empty string.
        // - Any changes to command syntax must be reflected here to avoid stale
        // hints.
        inline std::string hint_for_keyword(const std::string& kw) {
            if (kw == ":set")
                return " <variable> <value | solve <eq>>";
            if (kw == ":unset")
                return " <variable>";
            if (kw == ":ls")
                return " [pattern]";
            if (kw == ":solve")
                return " <lhs> = <rhs>";
            if (kw == ":simplify")
                return " <lhs> = <rhs> [-vars x y] [-isolated] [-fraction]";
            if (kw == ":expand")
                return " <expression>";
            if (kw == ":factor")
                return " <polynomial>";
            if (kw == ":config")
                return " <list|get|set|path|reset>";
            if (kw == ":env")
                return " [list|load|save|new|delete]";
            if (kw == ":help")
                return " [command]";
            if (kw == ":cls" || kw == ":clear")
                return "  — clear screen";
            return "";
        }

    } // namespace detail

    // Registers the hint callback with the Replxx instance.
    // - Hints are only provided when the user is typing the command token
    // itself (i.e., before the first space).
    // - This avoids misleading completions for command arguments, which may
    // have complex or context-sensitive syntax.
    // - The callback is performance-sensitive: invoked on every keystroke, so
    // must remain lightweight.
    // - The hint delay is set to 300ms to balance responsiveness and avoid
    // flicker.
    // - Any changes to command parsing logic elsewhere must be kept in sync
    // with the tokenization here.
    inline void setup_hints(replxx::Replxx& rx) {
        using std::string;

        rx.set_hint_callback(
            [](const string&          input,
               int&                   contextLen,
               replxx::Replxx::Color& color) -> replxx::Replxx::hints_t {
                replxx::Replxx::hints_t hints;
                color          = replxx::Replxx::Color::GRAY;

                string trimmed = input;
                size_t s       = trimmed.find_first_not_of(" \t");
                if (s == string::npos)
                    return hints;
                trimmed    = trimmed.substr(s);
                contextLen = 0;

                // Only emit a hint if the user is still typing the command
                // keyword. This avoids suggesting argument syntax in the middle
                // of a partially typed argument.
                if (trimmed.find(' ') == string::npos) {
                    string h = detail::hint_for_keyword(trimmed);
                    if (!h.empty())
                        hints.emplace_back(std::move(h));
                }

                return hints;
            });

        rx.set_hint_delay(300);
    }

} // namespace math_solver
