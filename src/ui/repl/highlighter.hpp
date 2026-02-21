#pragma once

#include <cctype>
#include <replxx.hxx>
#include <string>
#include <unordered_set>

namespace math_solver {

    // O(1) lookup for valid commands. Invariant: valid_cmds must be kept in
    // sync with the command parser. If a command is added/removed elsewhere,
    // update here.
    inline bool is_valid_command(const std::string& cmd) {
        static const std::unordered_set<std::string> valid_cmds = {
            ":",      ":set",      ":unset",  ":rm",     ":cls",     ":clear",
            ":ls",    ":help",     ":h",      ":config", ":conf",    ":env",
            ":solve", ":simplify", ":expand", ":factor", ":history", ":redo",
            ":load",  "exit",      "quit",    "q"};
        return valid_cmds.find(cmd) != valid_cmds.end();
    }

    inline bool is_operator(char c) {
        return c == '+' || c == '-' || c == '*' || c == '/' || c == '^' ||
               c == '=';
    }

    // Highlighter callback for REPL input. Maintains correctness invariants
    // for:
    // - Command recognition (must match is_valid_command)
    // - Numeric literals (at most one decimal point)
    // - Parenthesis balancing (tracks open/close, flags excess closing)
    // - Operator and symbol classification
    // - Any unrecognized character is considered an error (RED)
    //
    // Performance: Single pass, O(n) in input size. No heap allocations except
    // for temporary substrings (could be optimized if necessary).
    //
    // Subtlety: open_parens is local to the input line; does not track
    // multi-line state.
    inline void setup_highlighter(replxx::Replxx& rx) {
        rx.set_highlighter_callback([](const std::string&        input,
                                       replxx::Replxx::colors_t& colors) {
            int open_parens = 0;

            for (size_t i = 0; i < input.size(); ++i) {
                char c = input[i];

                // Command detection: Only at start of line or after whitespace.
                // Accepts both ':'-prefixed and bare commands like "exit".
                if (c == ':' || (std::isalpha(c) && i == 0)) {
                    size_t start = i;
                    while (i < input.size() && !std::isspace(input[i])) {
                        i++;
                    }

                    std::string           word = input.substr(start, i - start);

                    // Only color as command if ':'-prefixed or matches known
                    // exit/quit forms.
                    replxx::Replxx::Color color;
                    if (word[0] == ':' || word == "exit" || word == "quit" ||
                        word == "q") {
                        color = is_valid_command(word)
                                    ? replxx::Replxx::Color::GREEN
                                    : replxx::Replxx::Color::RED;
                    } else {
                        color = replxx::Replxx::Color::DEFAULT;
                    }

                    for (size_t j = start; j < i; ++j) {
                        colors[j] = color;
                    }
                    i--;
                }
                // Numeric literal: Accepts digits and at most one '.'.
                // Multiple '.' is flagged as error (RED).
                else if (std::isdigit(c) || c == '.') {
                    size_t start     = i;
                    int    dot_count = 0;

                    while (i < input.size() &&
                           (std::isdigit(input[i]) || input[i] == '.')) {
                        if (input[i] == '.')
                            dot_count++;
                        i++;
                    }

                    replxx::Replxx::Color color =
                        (dot_count <= 1) ? replxx::Replxx::Color::BROWN
                                         : replxx::Replxx::Color::RED;
                    for (size_t j = start; j < i; ++j) {
                        colors[j] = color;
                    }
                    i--;
                }
                // Operators: Only recognized set is colored.
                else if (is_operator(c)) {
                    colors[i] = replxx::Replxx::Color::CYAN;
                }
                // Parenthesis: Tracks open/close balance for error
                // highlighting.
                else if (c == '(') {
                    colors[i] = replxx::Replxx::Color::MAGENTA;
                    open_parens++;
                } else if (c == ')') {
                    if (open_parens > 0) {
                        colors[i] = replxx::Replxx::Color::MAGENTA;
                        open_parens--;
                    } else {
                        // Excess closing parenthesis: syntax error.
                        colors[i] = replxx::Replxx::Color::RED;
                    }
                }
                // Identifiers, whitespace, and underscores are left as default.
                else if (std::isspace(c) || std::isalpha(c) || c == '_') {
                    // No-op.
                }
                // Any other character is considered invalid in this context.
                else {
                    colors[i] = replxx::Replxx::Color::RED;
                }
            }
        });
    }

} // namespace math_solver