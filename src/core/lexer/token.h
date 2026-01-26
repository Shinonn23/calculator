#ifndef TOKEN_H
#define TOKEN_H

namespace math_solver {

    enum class TokenType {
        End,
        Number,
        Plus,
        Minus,
        Mul,
        Div,
        Pow,
        LParen,
        RParen,
    };

    struct Token {
        TokenType type;
        double    value; // used only if type == Number
    };

    inline const char* token_type_name(TokenType type) {
        switch (type) {
        case TokenType::End:
            return "end of input";
        case TokenType::Number:
            return "number";
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
        default:
            return "unknown";
        }
    }

} // namespace math_solver

#endif