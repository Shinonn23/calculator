#ifndef ERROR_H
#define ERROR_H

#include "span.h"
#include <stdexcept>
#include <string>
#include <vector>

namespace math_solver {

    // Base class for all math solver errors with span information
    class MathError : public std::runtime_error {
        protected:
        Span        span_;
        std::string input_;

        public:
        MathError(const std::string& message, const Span& span = Span(),
                  const std::string& input = "")
            : std::runtime_error(message), span_(span), input_(input) {}

        const Span&        span() const { return span_; }
        const std::string& input() const { return input_; }

        void        set_input(const std::string& input) { input_ = input; }

        std::string format() const {
            if (input_.empty()) {
                return std::string("Error: ") + what();
            }
            return format_error_at_span(what(), input_, span_);
        }
    };

    // Lexer/Parser errors
    class ParseError : public MathError {
        public:
        ParseError(const std::string& message, const Span& span = Span(),
                   const std::string& input = "")
            : MathError(message, span, input) {}
    };

    // Variable not found in context
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

    // Non-linear equation error
    class NonLinearError : public MathError {
        public:
        NonLinearError(const std::string& message, const Span& span = Span(),
                       const std::string& input = "")
            : MathError(message, span, input) {}
    };

    // Multiple unknowns in solve
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

    // No solution exists
    class NoSolutionError : public MathError {
        public:
        NoSolutionError(const std::string& message = "equation has no solution",
                        const Span&        span    = Span(),
                        const std::string& input   = "")
            : MathError(message, span, input) {}
    };

    // Domain constraint violation (root excluded by domain)
    class DomainError : public MathError {
        public:
        DomainError(const std::string& message, const Span& span = Span(),
                    const std::string& input = "")
            : MathError(message, span, input) {}
    };

    // Numerical solver diverged (all starting points failed)
    class SolverDivergedError : public MathError {
        public:
        SolverDivergedError(
            const std::string& message =
                "numerical solver diverged from all starting points",
            const Span& span = Span(), const std::string& input = "")
            : MathError(message, span, input) {}
    };

    // Infinite solutions
    class InfiniteSolutionsError : public MathError {
        public:
        InfiniteSolutionsError(
            const std::string& message = "equation has infinite solutions",
            const Span& span = Span(), const std::string& input = "")
            : MathError(message, span, input) {}
    };

    // Invalid equation format
    class InvalidEquationError : public MathError {
        public:
        InvalidEquationError(const std::string& message,
                             const Span&        span  = Span(),
                             const std::string& input = "")
            : MathError(message, span, input) {}
    };

    // Reserved keyword used as identifier
    class ReservedKeywordError : public MathError {
        public:
        ReservedKeywordError(const std::string& keyword,
                             const Span&        span  = Span(),
                             const std::string& input = "")
            : MathError("'" + keyword + "' is a reserved keyword", span,
                        input) {}
    };

} // namespace math_solver

#endif
