#pragma once

#include "ast/math/equation_expr.hpp"
#include "ast/math/expr.hpp"
#include "core/error.hpp"
#include "lexer/math/math_lexer.hpp"
#include "lexer/math/math_token.hpp"
#include <string>

namespace math_solver {

    class Parser {
        private:
        Lexer       lexer_;
        Token       current_;
        std::string input_;

        // Advances to the next token. Must be called after consuming a token to
        // maintain parser invariants. Assumes lexer_ is always ahead of
        // current_.
        void        advance() { current_ = lexer_.next_token(); }

        // Checks that the current token matches the expected type, otherwise
        // emits a ParseError with context. This is the main guard against
        // malformed input; all parser routines rely on this for error recovery.
        void        expect(TokenType type, const std::string& msg) {
            if (current_.type != type) {
                throw ParseError(msg, current_.span, input_);
            }
            advance();
        }

        // Precedence climbing parser entry points.
        // Each function is responsible for a specific precedence level.
        // Assumes input is well-formed up to the current token.
        ExprPtr parse_primary();
        ExprPtr parse_unary();
        ExprPtr parse_power();
        ExprPtr parse_multiplicative();
        ExprPtr parse_additive();
        ExprPtr parse_expression();

        public:
        // Initializes the parser and primes the first token.
        // The input string must remain valid for the lifetime of the parser.
        explicit Parser(const std::string& input)
            : lexer_(input), input_(input) {
            current_ = lexer_.next_token();
        }

        // Returns the original input string. Used for diagnostics and error
        // reporting.
        const std::string&              input() const;

        // Parses a single expression from the input.
        // Leaves the parser at the next unconsumed token.
        ExprPtr                         parse();

        // Attempts to parse either an expression or an equation.
        // Used by the top-level driver to disambiguate input.
        std::pair<ExprPtr, EquationPtr> parse_expression_or_equation();

        // Parses an equation. Assumes the input contains an '=' token.
        // Returns nullptr if no equation is found.
        EquationPtr                     parse_equation();
    };
} // namespace math_solver
