#pragma once

#include "command_diag.hpp"
#include "core/error.hpp"
#include "runtime/context/context.hpp"
#include "ui/color.hpp"
#include "ui/suggestions.hpp"

#include <iostream>

namespace math_solver {
    namespace diag {

        // Augments an UndefinedVariableError with a variable name suggestion
        // from the current context, then emits it.
        // Called at every catch site for UndefinedVariableError in math
        // handlers.
        inline void emit_undefined_var(const UndefinedVariableError& e,
                                       const Context&                ctx) {
            if (auto m = suggest(e.var_name(), ctx.all_names()))
                e.with_help("a variable with a similar name exists: `" + *m +
                            "`");
            std::cout << e.format();
        }

        // Emits a MathError. If it is a NonLinearError, appends an inline hint
        // pointing the user toward :set for variable definition.
        // Used in do_solve and do_simplify where nonlinear errors are
        // recoverable.
        inline void emit_math_error_with_hint(const MathError& e) {
            std::cout << e.format();
            if (dynamic_cast<const NonLinearError*>(&e))
                std::cout << ansi::dim
                          << "  Hint: use :set to define variables first\n"
                          << ansi::reset;
        }

        // Emits a plain MathError with no additional hints.
        // Used where NonLinearError is not expected or not actionable.
        inline void emit_math_error(const MathError& e) {
            std::cout << e.format();
        }

        // Emits a usage line when payload is empty.
        // Called as the first guard in every do_* function.
        inline void emit_usage(const std::string& usage) {
            std::cout << "  Usage: " << usage << "\n";
        }

        // Emits an unknown math command diagnostic.
        // `bad_cmd` is the unrecognized token extracted from raw input.
        inline void emit_unknown_math_command(const std::string& raw,
                                              const std::string& bad_cmd) {
            UnknownCommandError e(bad_cmd, find_token_span(raw, bad_cmd), raw);
            std::cout << e.format() << "\n";
        }

        // Last-resort catch for std::exception in evaluate.
        // Not a DiagnosticBuilder path — these are unexpected runtime errors
        // that don't have span information.
        inline void emit_runtime_exception(const std::exception& e) {
            std::cout << ansi::red << "  Error: " << ansi::reset << e.what()
                      << "\n";
        }

    } // namespace diag
} // namespace math_solver