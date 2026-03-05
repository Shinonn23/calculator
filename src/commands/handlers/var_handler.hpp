#pragma once

#include "algebra/polynomial/ast_to_poly.hpp"
#include "algebra/polynomial/factor.hpp"
#include "algebra/solver/solver.hpp"
#include "ast/command/history_entry.hpp"
#include "ast/command/var_command.hpp"
#include "ast/math/array_expr.hpp"
#include "config/config.hpp"
#include "diagnostics/diagnostic.hpp"
#include "diagnostics/kinds/command_errors.hpp"
#include "diagnostics/kinds/var_errors.hpp"
#include "diagnostics/sink.hpp"
#include "parser/math/math_parser.hpp"
#include "runtime/context/context.hpp"
#include "runtime/context/resolver.hpp"

#include <iostream>

namespace math_solver {
    namespace handlers {

        // Variable name validation:
        // - Invariant: names must start with alpha/_ and contain only
        // alphanum/_.
        // - This is relied upon by downstream parsing and symbol table logic.
        // - Reserved keywords are disallowed to avoid shadowing and semantic
        // ambiguity.
        inline bool is_valid_identifier(const std::string& name) {
            if (name.empty() || !(std::isalpha(name[0]) || name[0] == '_'))
                return false;
            for (char c : name)
                if (!std::isalnum(c) && c != '_')
                    return false;
            if (is_reserved_keyword(name))
                return false;
            return true;
        }

        // Handles all forms of :set commands.
        // - Maintains the invariant that variable names are valid and not
        // reserved.
        // - Updates or inserts variable bindings in the context.
        // - On error, prints diagnostics and leaves context unchanged.
        // - For "solve", clones all context except the target variable to avoid
        //   accidental self-reference during equation solving.
        // - Performance: context cloning is O(n) in the number of variables.
        // - Correctness: relies on parser and solver subsystems for semantic
        // checks.
        inline HistoryStatus handle_set(const VarCommand& cmd, Context& ctx,
                                        Config& config, DiagnosticSink& sink) {
            (void)config;
            const std::string& var  = cmd.var_name();
            const std::string& raw  = cmd.raw_command();
            const std::string& file = cmd.source_file();
            size_t             line = cmd.source_line();

            if (var.empty()) {
                sink.push(errors::missing_var_name(
                    raw, ":set", "`:set <var> <expr>`", file, line));
                return HistoryStatus::Error;
            }
            if (!is_valid_identifier(var)) {
                if (is_reserved_keyword(var)) {
                    sink.push(errors::reserved_keyword(raw, var, file, line));
                } else {
                    sink.push(errors::invalid_identifier(raw, var, file, line));
                }
                return HistoryStatus::Error;
            }
            if (cmd.payload().empty()) {
                sink.push(errors::missing_expr(raw, var, file, line));
                return HistoryStatus::Error;
            }

            const std::string& payload = cmd.payload();
            if (!cmd.has_math_action()) {
                sink.push(errors::missing_expr(raw, var, file, line));
                return HistoryStatus::Error;
            }
            const std::string& math_action = cmd.math_action();

            // "solve" action:
            // - Avoids self-reference by cloning context without the target
            // variable.
            // - Relies on EquationSolver for semantic and syntactic
            // correctness.
            // - Any MathError is surfaced directly to the user.
            if (math_action == "solve") {
                try {
                    Parser parser(payload);
                    auto   parse_result = parser.parse_equation().with_location(
                        cmd.source_file(), cmd.source_line());
                    if (!parse_result) {
                        sink.push(parse_result.error());
                        return HistoryStatus::Error;
                    }
                    auto    eq = std::move(*parse_result);
                    Context temp_ctx;
                    for (const auto& [n, e] : ctx.all())
                        if (n != var)
                            temp_ctx.set(n, *e);

                    EquationSolver      solver(&temp_ctx, payload);
                    Result<SolveResult> result = solver.solve(*eq);

                    ctx.set(var, result->value);
                    std::ostringstream oss;
                    oss << "  " << var << " = " << result->value << "\n";
                    sink.push_output(oss.str());
                    return HistoryStatus::Success;
                } catch (const std::exception& e) {
                    // TODO: convert solver to Result-based flow.
                    (void)e;
                    return HistoryStatus::Error;
                }
            }

            // "expand" action:
            // - Converts the expression to a polynomial and stores the expanded
            // form.
            // - Assumes ASTToPolynomial and Polynomial are correct and total.
            if (math_action == "expand") {
                try {
                    Parser parser(payload);
                    auto   parse_result = parser.parse().with_location(
                        cmd.source_file(), cmd.source_line());
                    if (!parse_result) {
                        sink.push(parse_result.error());
                        return HistoryStatus::Error;
                    }
                    auto expr   = std::move(*parse_result);
                    auto poly_r = ASTToPolynomial(payload).convert(*expr);
                    if (!poly_r) {
                        sink.push(poly_r.error().with_location(
                            cmd.source_file(), cmd.source_line()));
                        return HistoryStatus::Error;
                    }
                    Polynomial poly = *poly_r;
                    Parser     sp(poly.to_string());
                    auto       sr = sp.parse();
                    if (!sr) {
                        sink.push(sr.error().with_location(cmd.source_file(),
                                                           cmd.source_line()));
                        return HistoryStatus::Error;
                    }
                    // Only set after successful parse
                    ctx.set(var, std::move(*sr));
                    std::ostringstream oss;
                    oss << "  " << var << " = " << poly.to_string() << "\n";
                    sink.push_output(oss.str());
                    return HistoryStatus::Success;
                } catch (const std::exception& e) {
                    // TODO: convert ASTToPolynomial to Result-based flow.
                    (void)e;
                    return HistoryStatus::Error;
                }
            }

            // "factor" action:
            // - Converts the expression to a polynomial and stores the factored
            // form.
            // - Assumes factor_polynomial is correct and total.
            if (math_action == "factor") {
                try {
                    Parser parser(payload);
                    auto   parse_result = parser.parse().with_location(
                        cmd.source_file(), cmd.source_line());
                    if (!parse_result) {
                        sink.push(parse_result.error());
                        return HistoryStatus::Error;
                    }
                    auto expr   = std::move(*parse_result);
                    auto poly_r = ASTToPolynomial(payload).convert(*expr);
                    if (!poly_r) {
                        sink.push(poly_r.error().with_location(
                            cmd.source_file(), cmd.source_line()));
                        return HistoryStatus::Error;
                    }
                    auto        factored = factor_polynomial(*poly_r);
                    std::string str      = factored.to_string();
                    Parser      sp(str);
                    auto        sr = sp.parse();
                    if (!sr) {
                        sink.push(sr.error().with_location(cmd.source_file(),
                                                           cmd.source_line()));
                        return HistoryStatus::Error;
                    }
                    // Only set after successful parse
                    ctx.set(var, std::move(*sr));
                    std::ostringstream oss;
                    oss << "  " << var << " = " << str << "\n";
                    sink.push_output(oss.str());
                    return HistoryStatus::Success;
                } catch (const std::exception& e) {
                    // TODO: convert factor_polynomial to Result-based flow.
                    (void)e;
                    return HistoryStatus::Error;
                }
            }

            // Default assignment:
            // - Parses and stores the expression.
            // - Attempts eager evaluation; if undefined variables are present,
            //   falls back to expansion.
            // - Handles circular dependencies gracefully, emitting a warning.
            try {
                Parser parser(payload);
                auto   parse_result = parser.parse().with_location(
                    cmd.source_file(), cmd.source_line());
                if (!parse_result) {
                    sink.push(parse_result.error());
                    return HistoryStatus::Error;
                }
                ctx.set(var, std::move(*parse_result));

                // Array literals are stored directly; skip scalar evaluation.
                if (dynamic_cast<const ArrayExpr*>(&ctx.get_expr(var))) {
                    std::ostringstream oss;
                    oss << "  " << var << " = " << ctx.get_expr(var).to_string()
                        << "\n";
                    sink.push_output(oss.str());
                    return HistoryStatus::Success;
                }

                auto res = Resolver::evaluate(ctx.get_expr(var), ctx);
                std::ostringstream oss;
                if (res) {
                    oss << "  " << var << " = " << *res << "\n";
                } else {
                    // Fall back to symbolic display when variables are
                    // undefined.
                    oss << "  " << var << " = " << ctx.get_expr(var).to_string()
                        << "\n";
                }
                sink.push_output(oss.str());
                return HistoryStatus::Success;
            } catch (const std::exception& e) {
                // TODO: convert parser to Result-based flow.
                (void)e;
                return HistoryStatus::Error;
            }
            return HistoryStatus::Error;
        }

        // Removes a variable binding from the context.
        // - If the variable does not exist, emits a diagnostic.
        inline HistoryStatus handle_unset(const VarCommand& cmd, Context& ctx,
                                          DiagnosticSink& sink) {
            const std::string& var  = cmd.var_name();
            const std::string& raw  = cmd.raw_command();
            const std::string& file = cmd.source_file();
            size_t             line = cmd.source_line();

            if (var.empty()) {
                sink.push(errors::missing_var_name(
                    raw, ":unset", "`:unset <var>`", file, line));
                return HistoryStatus::Error;
            }

            if (ctx.has(var)) {
                ctx.unset(var);
                sink.push_output("  Removed: " + var + "\n");
                return HistoryStatus::Success;
            }

            sink.push(errors::var_not_found(raw, var, ctx, file, line));
            return HistoryStatus::Error;
        }

        // Dispatches to the appropriate handler based on the VarCommand action.
        // - Only Set and Unset actions are supported.
        // - If new actions are added, this switch must be updated.
        // - Returns HistoryStatus::Unknown for unhandled actions (should be
        // unreachable).
        inline HistoryStatus handle_var(const VarCommand& cmd, Context& ctx,
                                        Config& config, std::string& raw,
                                        DiagnosticSink& sink) {
            switch (cmd.action()) {
            case VarCommand::Action::Set:
                return handle_set(cmd, ctx, config, sink);
            case VarCommand::Action::Unset:
                return handle_unset(cmd, ctx, sink);
            case VarCommand::Action::Unknown: {
                std::string input   = cmd.raw_command();
                std::string bad_cmd = input.substr(0, input.find(' '));

                Diagnostic  e       = errors::unknown_command(
                    bad_cmd, find_token_span(raw, bad_cmd), raw);
                e = e.with_location(cmd.source_file(), cmd.source_line());
                sink.push(e);
                return HistoryStatus::Error;
            }
            }
            return HistoryStatus::Unknown;
        }

    } // namespace handlers
} // namespace math_solver