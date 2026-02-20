#pragma once

#include "core/span.hpp"
#include <string>

namespace math_solver {

    // TokenType encodes the lexical categories recognized by the math lexer.
    // - The set is intentionally minimal; extensions should consider parser
    // impact.
    // - The ordering is not semantically significant.
    enum class TokenType {
        End,
        Number,
        Identifier,
        Plus,
        Minus,
        Mul,
        Div,
        Pow,
        LParen,
        RParen,
        Equals,
    };

    struct Token {
        TokenType   type;
        double      value; // Only valid if type == Number.
        std::string name;  // Only valid if type == Identifier.
        Span        span;  // Always set; denotes source location.

        // Default constructor yields End token at dummy span.
        Token() : type(TokenType::End), value(0), name(), span() {}

        // Constructs a numeric token; name is left empty.
        Token(TokenType t, double v, const Span& s = Span())
            : type(t), value(v), name(), span(s) {}

        // Constructs an identifier token; value is zeroed.
        Token(TokenType t, const std::string& n, const Span& s = Span())
            : type(t), value(0), name(n), span(s) {}
    };

    // Returns a stable string representation for diagnostics and debugging.
    // - Used in error messages and pretty-printing.
    // - Must remain in sync with TokenType.
    inline const char* token_type_name(TokenType type) {
        switch (type) {
        case TokenType::End:
            return "end of input";
        case TokenType::Number:
            return "number";
        case TokenType::Identifier:
            return "identifier";
        case TokenType::Plus:
            return "+";
        case TokenType::Minus:
            return "-";
        case TokenType::Mul:
            return "*";
        case TokenType::Div:
            return "/";
        case TokenType::Pow:
            return "^";
        case TokenType::LParen:
            return "(";
        case TokenType::RParen:
            return ")";
        case TokenType::Equals:
            return "=";
        default:
            return "unknown";
        }
    }

    // Returns true if `name` is a reserved keyword and cannot be used as an
    // identifier.
    // - The set of reserved words is fixed here; changes require parser
    // coordination.
    // - Used to prevent shadowing of command names in user input.
    inline bool is_reserved_keyword(const std::string& name) {
        return name == "simplify" || name == "solve" || name == "set" ||
               name == "unset" || name == "clear" || name == "help" ||
               name == "exit" || name == "quit" || name == "config" ||
               name == "env" || name == "expand" || name == "factor";
    }

} // namespace math_solver
