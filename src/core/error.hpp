#pragma once

#include "span.hpp"
#include <stdexcept>
#include <string>
#include <vector>

namespace math_solver {

    // Base error type for all math_solver errors.
    // - Carries source span and input context for diagnostics.
    // - Invariant: span_ and input_ must correspond to the error site in the
    // original input.
    // - Used throughout the parser, type checker, and solver to propagate
    // user-facing errors.
    class MathError : public std::runtime_error {
        protected:
        Span        span_;
        std::string input_;

        public:
        MathError(const std::string& message,
                  const Span&        span  = Span(),
                  const std::string& input = "")
            : std::runtime_error(message), span_(span), input_(input) {}

        const Span&        span() const { return span_; }
        const std::string& input() const { return input_; }

        void        set_input(const std::string& input) { input_ = input; }

        // Formats the error for user diagnostics.
        // - If input_ is empty, falls back to a generic message.
        // - Otherwise, emits a span-highlighted error.
        std::string format() const {
            if (input_.empty()) {
                return std::string("Error: ") + what();
            }
            return format_error_at_span(what(), input_, span_);
        }
    };

    // Raised for all lexer/parser failures.
    // - Used to signal unrecoverable parse errors.
    // - Invariant: Should only be constructed when the input cannot be parsed
    // further.
    class ParseError : public MathError {
        public:
        ParseError(const std::string& message,
                   const Span&        span  = Span(),
                   const std::string& input = "")
            : MathError(message, span, input) {}
    };

    // Raised when a variable is referenced but not present in the current
    // context.
    // - var_name_ must match the identifier as written in the input.
    // - Used by the resolver and evaluation passes.
    class UndefinedVariableError : public MathError {
        private:
        std::string var_name_;

        public:
        UndefinedVariableError(const std::string& var_name,
                               const Span&        span  = Span(),
                               const std::string& input = "")
            : MathError("undefined variable '" + var_name + "'", span, input),
              var_name_(var_name) {}

        const std::string& var_name() const { return var_name_; }
    };

    // Raised when the equation is detected to be non-linear.
    // - Used to reject equations that cannot be handled by the linear solver.
    // - Invariant: Only constructed after non-linearity is proven.
    class NonLinearError : public MathError {
        public:
        NonLinearError(const std::string& message,
                       const Span&        span  = Span(),
                       const std::string& input = "")
            : MathError(message, span, input) {}
    };

    // Raised when more than one unknown is present in a solve request.
    // - unknowns_ must be non-empty and contain all unknown identifiers.
    // - Used to enforce single-unknown constraint in the solver.
    class MultipleUnknownsError : public MathError {
        private:
        std::vector<std::string> unknowns_;

        public:
        MultipleUnknownsError(const std::vector<std::string>& unknowns,
                              const Span&                     span  = Span(),
                              const std::string&              input = "")
            : MathError(build_message(unknowns), span, input),
              unknowns_(unknowns) {}

        const std::vector<std::string>& unknowns() const { return unknowns_; }

        private:
        // Constructs a diagnostic message listing all unknowns.
        static std::string build_message(const std::vector<std::string>& vars) {
            std::string msg = "multiple unknowns in equation (";
            for (size_t i = 0; i < vars.size(); ++i) {
                if (i > 0)
                    msg += ", ";
                msg += vars[i];
            }
            msg += ")";
            return msg;
        }
    };

    // Raised when the equation is unsatisfiable.
    // - Used by the solver when contradiction is detected.
    class NoSolutionError : public MathError {
        public:
        NoSolutionError(const std::string& message = "equation has no solution",
                        const Span&        span    = Span(),
                        const std::string& input   = "")
            : MathError(message, span, input) {}
    };

    // Raised when the equation admits infinitely many solutions.
    // - Used by the solver when the system is underconstrained.
    class InfiniteSolutionsError : public MathError {
        public:
        InfiniteSolutionsError(
            const std::string& message = "equation has infinite solutions",
            const Span&        span    = Span(),
            const std::string& input   = "")
            : MathError(message, span, input) {}
    };

    // Raised when the input does not conform to the expected equation format.
    // - Used by the parser and pre-solver validation.
    class InvalidEquationError : public MathError {
        public:
        InvalidEquationError(const std::string& message,
                             const Span&        span  = Span(),
                             const std::string& input = "")
            : MathError(message, span, input) {}
    };

    // Raised when a reserved keyword is used as an identifier.
    // - Used by the lexer and parser to enforce language constraints.
    class ReservedKeywordError : public MathError {
        public:
        ReservedKeywordError(const std::string& keyword,
                             const Span&        span  = Span(),
                             const std::string& input = "")
            : MathError(
                  "'" + keyword + "' is a reserved keyword", span, input) {}
    };

    // Raised when a variable depends on itself, directly or transitively.
    // - var_name_ must be the variable involved in the cycle.
    // - Used by the dependency analysis pass.
    class CircularDependencyError : public MathError {
        private:
        std::string var_name_;

        public:
        CircularDependencyError(const std::string& var_name,
                                const Span&        span  = Span(),
                                const std::string& input = "")
            : MathError("circular variable dependency on '" + var_name + "'",
                        span,
                        input),
              var_name_(var_name) {}

        const std::string& var_name() const { return var_name_; }
    };

} // namespace math_solver
