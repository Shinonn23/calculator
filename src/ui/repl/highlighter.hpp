#pragma once

#include "ast/math/call_expr.hpp"
#include "parser/command/command_parser_registry.hpp"
#include <cctype>
#include <replxx.hxx>
#include <string>

namespace math_solver {

    // Validates a command word against the parser registry — the single source
    // of truth. Accepts both ':'-prefixed forms (":solve") and bare aliases
    // ("exit" → ":exit"). A bare colon (":") is treated as in-progress input
    // rather than an error.
    inline bool is_valid_command(const std::string& cmd) {
        if (cmd == ":")
            return true; // still typing; don't flag as error
        const auto& names = registered_command_names();
        if (names.count(cmd))
            return true;
        // Allow bare aliases: "exit" → ":exit", "quit" → ":quit", etc.
        if (cmd[0] != ':' && names.count(":" + cmd))
            return true;
        return false;
    }

    inline bool is_operator(char c) {
        return c == '+' || c == '-' || c == '*' || c == '/' || c == '^' ||
               c == '=' || c == '!';
    }

    // Highlighter callback for REPL input. Maintains correctness invariants
    // for:
    // - Command recognition (must match is_valid_command)
    // - Numeric literals (at most one decimal point)
    // - Parenthesis and bracket balancing (tracks open/close, flags excess
    //   closing parens)
    // - Built-in function names highlighted in YELLOW
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

                // Command detection: Only at start of line.
                // Accepts both ':'-prefixed and bare commands like "exit".
                if (c == ':' || (std::isalpha(static_cast<unsigned char>(c)) &&
                                 i == 0)) {
                    size_t start = i;
                    while (i < input.size() && !std::isspace(input[i])) {
                        i++;
                    }

                    std::string word = input.substr(start, i - start);

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
                else if (std::isdigit(static_cast<unsigned char>(c)) ||
                         c == '.') {
                    size_t start     = i;
                    int    dot_count = 0;

                    while (i < input.size() &&
                           (std::isdigit(static_cast<unsigned char>(input[i])) ||
                            input[i] == '.')) {
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
                // Mid-expression identifiers: scan full token and detect
                // built-in function names (highlighted YELLOW).
                else if (std::isalpha(static_cast<unsigned char>(c)) ||
                         c == '_') {
                    size_t start = i;
                    while (i < input.size() &&
                           (std::isalnum(static_cast<unsigned char>(input[i])) ||
                            input[i] == '_')) {
                        i++;
                    }
                    std::string word = input.substr(start, i - start);
                    if (func_kind_from_name(word).has_value()) {
                        for (size_t j = start; j < i; ++j) {
                            colors[j] = replxx::Replxx::Color::YELLOW;
                        }
                    }
                    // Variable names and other identifiers: DEFAULT (no-op).
                    i--;
                }
                // Operators: recognized set is colored CYAN.
                else if (is_operator(c)) {
                    colors[i] = replxx::Replxx::Color::CYAN;
                }
                // Parenthesis: tracks open/close balance for error
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
                // Brackets: colored MAGENTA (used for array literals).
                // No balance tracking — mismatched brackets are caught by the
                // parser, not highlighted here.
                else if (c == '[' || c == ']') {
                    colors[i] = replxx::Replxx::Color::MAGENTA;
                }
                // Comma and semicolon: separators, leave as DEFAULT.
                else if (c == ',' || c == ';') {
                    // No-op.
                }
                // Whitespace: leave as DEFAULT.
                else if (std::isspace(static_cast<unsigned char>(c))) {
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