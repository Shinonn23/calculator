#ifndef TOKEN_H
#define TOKEN_H

#include "../common/span.hpp"
#include <string>

namespace math_solver {

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
        double      value; // used only if type == Number
        std::string name;  // used only if type == Identifier
        Span        span;  // position in input

        Token() : type(TokenType::End), value(0), name(), span() {}

        Token(TokenType t, double v, const Span& s = Span())
            : type(t), value(v), name(), span(s) {}

        Token(TokenType t, const std::string& n, const Span& s = Span())
            : type(t), value(0), name(n), span(s) {}
    };

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

    // Reserved keywords that cannot be used as identifiers
    inline bool is_reserved_keyword(const std::string& name) {
        return name == "simplify" || name == "solve" || name == "set" ||
               name == "unset" || name == "clear" || name == "help" ||
               name == "exit" || name == "quit" || name == "config" ||
               name == "env";
    }

} // namespace math_solver

#endif