#pragma once

#include "ast/math/equation_expr.hpp"
#include "ast/math/expr.hpp"
#include "diagnostics/diagnostic.hpp"
#include "lexer/math/math_lexer.hpp"
#include "lexer/math/math_token.hpp"
#include <optional>
#include <string>

namespace math_solver {

    class Parser {
        private:
        Lexer                     lexer_;
        Token                     current_;
        std::string               input_;
        std::string               raw_cmd_;
        std::optional<Diagnostic> last_error_;

        // Advances to the next token. Must be called after consuming a token to
        // maintain parser invariants. Assumes lexer_ is always ahead of
        // current_.
        bool                      advance() {
            auto t = lexer_.next_token();
            if (!t) {
                if (!last_error_)
                    last_error_ = t.error();
                return false;
            }
            current_ = *t;
            return true;
        }

        // Checks that the current token matches the expected type, otherwise
        // emits a ParseError with context. This is the main guard against
        // malformed input; all parser routines rely on this for error recovery.
        bool expect(TokenType type, const std::string& msg) {
            if (current_.type != type) {
                if (!last_error_)
                    last_error_ =
                        errors::parse(msg, current_.span, input_, raw_cmd_);
                return false;
            }
            (void)advance();
            return true;
        }

        // Precedence climbing parser entry points.
        // Each function is responsible for a specific precedence level.
        // Assumes input is well-formed up to the current token.
        Result<ExprPtr> parse_primary();
        Result<ExprPtr> parse_array_literal();
        Result<ExprPtr> parse_unary();
        Result<ExprPtr> parse_power();
        Result<ExprPtr> parse_multiplicative();
        Result<ExprPtr> parse_additive();
        Result<ExprPtr> parse_expression();

        public:
        // Initializes the parser and primes the first token.
        // The input string must remain valid for the lifetime of the parser.
        explicit Parser(const std::string& input,
                        const std::string& raw_cmd = "")
            : lexer_(input), input_(input), raw_cmd_(raw_cmd) {}

        // Returns the original input string. Used for diagnostics and error
        // reporting.
        const std::string&                      input() const;
        const std::string&                      raw_cmd() const;

        // Parses a single expression from the input.
        Result<ExprPtr>                         parse();

        // Attempts to parse either an expression or an equation.
        Result<std::pair<ExprPtr, EquationPtr>> parse_expression_or_equation();

        // Parses an equation. Assumes the input contains an '=' token.
        Result<EquationPtr>                     parse_equation();

        private:
        Result<ExprPtr> parse_impl();
        Result<std::pair<ExprPtr, EquationPtr>>
                            parse_expression_or_equation_impl();
        Result<EquationPtr> parse_equation_impl();
    };
} // namespace math_solver
