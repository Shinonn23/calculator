#pragma once

//! # Module — `src/commands/handlers/var_handler.hpp`
//!
//! Implements variable-management command handlers: `handle_set`,
//! `handle_unset`, and the top-level dispatcher `handle_var`. All functions are
//! `inline` and live entirely in this header. They operate on the live
//! `Context` and surface errors via `DiagnosticSink` rather than exceptions.

#include "algebra/polynomial/ast_to_poly.hpp"
#include "algebra/polynomial/factor.hpp"
#include "algebra/solver/solver.hpp"
#include "ast/command/history_entry.hpp"
#include "ast/command/var_command.hpp"
#include "ast/math/array_expr.hpp"
#include "ast/math/binary_expr.hpp"
#include "ast/math/call_expr.hpp"
#include "ast/math/unary_expr.hpp"
#include "ast/math/variable_expr.hpp"
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

        /// Return `true` when `name` is a syntactically valid, non-reserved
        /// identifier.
        ///
        /// A valid identifier starts with an ASCII letter or `'_'` and contains
        /// only ASCII alphanumeric characters or `'_'`. Reserved keywords (as
        /// determined by `is_reserved_keyword`) are rejected even if they
        /// satisfy the character rules.
        ///
        /// # Arguments
        ///
        /// * `name` — The candidate identifier string to validate.
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

        /// Walk an expression tree and return true if `target` appears as a
        /// Variable.
        ///
        /// Uses the ExprVisitor interface for full traversal. Short-circuits on
        /// the first match to avoid unnecessary work.
        class SelfRefChecker : public ExprVisitor {
            const std::string& target_;

            public:
            bool found = false;
            explicit SelfRefChecker(const std::string& target)
                : target_(target) {}

            void visit(const Number&) override {}
            void visit(const Variable& node) override {
                if (node.name() == target_)
                    found = true;
            }
            void visit(const BinaryOp& node) override {
                if (found)
                    return;
                node.left().accept(*this);
                if (found)
                    return;
                node.right().accept(*this);
            }
            void visit(const UnaryOp& node) override {
                if (found)
                    return;
                node.operand().accept(*this);
            }
            void visit(const FunctionCall& node) override {
                if (found)
                    return;
                node.arg().accept(*this);
            }
            void visit(const ArrayExpr& node) override {
                for (const auto& e : node.elements()) {
                    if (found)
                        return;
                    e->accept(*this);
                }
            }
        };

        inline bool expr_references_var(const Expr&        expr,
                                        const std::string& var) {
            SelfRefChecker checker(var);
            expr.accept(checker);
            return checker.found;
        }

        /// Execute a `:set <var> <expr|action>` command and bind the result in
        /// `ctx`.
        ///
        /// Dispatches on `cmd.math_action()`:
        /// - `"solve"` — parses the payload as an equation, solves it via
        ///   `EquationSolver` in a context that excludes the target variable to
        ///   prevent self-reference, and stores the numeric result.
        /// - `"expand"` — converts the expression to a `Polynomial` via
        ///   `ASTToPolynomial`, then re-parses and stores the expanded form.
        /// - `"factor"` — converts to `Polynomial`, applies
        /// `factor_polynomial`,
        ///   re-parses, and stores the factored form.
        /// - Default — stores the parsed expression. Attempts eager numeric
        ///   evaluation via `Resolver`; falls back to symbolic display when
        ///   variables are undefined.
        ///
        /// # Arguments
        ///
        /// * `cmd`    — The `:set` command node; supplies var name, payload,
        /// flags, and source info.
        /// * `ctx`    — Variable context; updated on success.
        /// * `config` — Configuration store (currently unused; reserved for
        /// future use).
        /// * `sink`   — Diagnostic sink for errors and output.
        ///
        /// # Returns
        ///
        /// `HistoryStatus::Success` when the variable is bound and output
        /// emitted, `HistoryStatus::Error` on any validation or solver failure.
        ///
        /// # Errors
        ///
        /// Pushes `missing_var_name` when the variable name is absent.
        /// Pushes `reserved_keyword` or `invalid_identifier` for invalid names.
        /// Pushes `missing_expr` when no payload is provided.
        /// Pushes parse and solver diagnostics for `"solve"` / `"expand"` /
        /// `"factor"` actions.
        inline HistoryStatus handle_set(const VarCommand& cmd, Context& ctx,
                                        Config& config, DiagnosticSink& sink) {
            (void)config;
            const std::vector<std::string>& vars = cmd.var_name();
            const std::string&              raw  = cmd.raw_command();
            const std::string&              file = cmd.source_file();
            size_t                          line = cmd.source_line();

            if (vars.empty()) {
                sink.push(errors::missing_var_name(
                    raw, find_token_span(raw, ":set"),
                    "`:set <var> <expr>`", file, line));
                return HistoryStatus::Error;
            }

            if (vars.size() > 1) {
                sink.push(errors::not_support_multiple(
                    raw, cmd.var_name_span(), vars, file, line));
                return HistoryStatus::Error;
            }

            const std::string& var = vars[0];

            if (var.empty()) {
                sink.push(errors::missing_var_name(
                    raw, find_token_span(raw, ":set"),
                    "`:set <var> <expr>`", file, line));
                return HistoryStatus::Error;
            }
            if (!is_valid_identifier(var)) {
                if (is_reserved_keyword(var)) {
                    sink.push(errors::reserved_keyword(
                        raw, cmd.var_name_span(), var, file, line));
                } else {
                    sink.push(errors::invalid_identifier(
                        raw, cmd.var_name_span(), var, file, line));
                }
                return HistoryStatus::Error;
            }
            if (cmd.payload().empty()) {
                sink.push(errors::missing_expr(
                    raw, cmd.var_name_span(), var, file, line));
                return HistoryStatus::Error;
            }

            const std::string& payload = cmd.payload();
            if (!cmd.has_math_action()) {
                sink.push(errors::missing_expr(
                    raw, cmd.var_name_span(), var, file, line));
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

                if (!result) {
                    sink.push(result.error());
                    return HistoryStatus::Error;
                }

                ctx.set(var, result->value);
                std::ostringstream oss;
                oss << "  " << var << " = " << result->value << "\n";
                sink.push_output(oss.str());
                return HistoryStatus::Success;
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

                    auto expr = std::move(*parse_result);

                    if (expr_references_var(*expr, var)) {
                        sink.push(errors::self_reference(
                            raw, cmd.var_name_span(), var, file, line));
                        return HistoryStatus::Error;
                    }
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
                Parser parser(payload);
                auto   parse_result = parser.parse().with_location(
                    cmd.source_file(), cmd.source_line());
                if (!parse_result) {
                    sink.push(parse_result.error());
                    return HistoryStatus::Error;
                }

                auto expr = std::move(*parse_result);

                if (expr_references_var(*expr, var)) {
                    sink.push(errors::self_reference(
                        raw, cmd.var_name_span(), var, file, line));
                    return HistoryStatus::Error;
                }

                auto poly_r = ASTToPolynomial(payload).convert(*expr);
                if (!poly_r) {
                    sink.push(poly_r.error().with_location(cmd.source_file(),
                                                           cmd.source_line()));
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
            }

            // Default assignment:
            // - Parses and stores the expression.
            // - Attempts eager evaluation; if undefined variables are present,
            //   falls back to expansion.
            // - Handles circular dependencies gracefully, emitting a warning.

            Parser parser(payload);
            auto parse_result = parser.parse().with_location(cmd.source_file(),
                                                             cmd.source_line());
            if (!parse_result) {
                sink.push(parse_result.error());
                return HistoryStatus::Error;
            }
            if (expr_references_var(**parse_result, var)) {
                sink.push(errors::self_reference(
                    raw, cmd.var_name_span(), var, file, line));
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

            auto               res = Resolver::evaluate(ctx.get_expr(var), ctx);
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

            return HistoryStatus::Error;
        }

        /// Remove one or more variable bindings from `ctx`.
        ///
        /// Iterates over all names in `cmd.var_name()`. Each name is removed
        /// from the live context if present; missing names produce a
        /// `var_not_found` diagnostic. If any removal fails the context is
        /// restored to its state before the call (transactional via clone).
        ///
        /// # Arguments
        ///
        /// * `cmd`  — The `:unset` command node; `var_name()` lists the
        /// targets.
        /// * `ctx`  — Variable context; mutated on success.
        /// * `sink` — Diagnostic sink for errors and output.
        ///
        /// # Returns
        ///
        /// `HistoryStatus::Success` when all named variables are removed,
        /// `HistoryStatus::Error` when any variable is missing or a name is
        /// empty (context is rolled back in this case).
        ///
        /// # Errors
        ///
        /// Pushes `missing_var_name` for empty name strings.
        /// Pushes `var_not_found` for names not present in `ctx`.
        inline HistoryStatus handle_unset(const VarCommand& cmd, Context& ctx,
                                          DiagnosticSink& sink) {
            const std::vector<std::string>& vars       = cmd.var_name();
            const std::string&              raw        = cmd.raw_command();
            const std::string&              file       = cmd.source_file();
            size_t                          line       = cmd.source_line();
            bool                            is_success = true;

            Context                         temp       = ctx.clone(ctx);

            for (const std::string& var : vars) {

                if (var.size() < 1) {
                    sink.push(errors::missing_var_name(
                        raw, find_token_span(raw, ":unset"),
                        "`:unset <var>, <var>, ...`", file, line));
                    is_success = false;
                }

                if (ctx.has(var)) {
                    ctx.unset(var);
                    sink.push_output("  Removed: " + var + "\n");
                    is_success = true;
                } else {
                    sink.push(errors::var_not_found(
                        raw, cmd.var_name_span(), var, ctx, file, line));
                    is_success = false;
                }
            }

            if (!is_success) {
                ctx = std::move(temp);
                return HistoryStatus::Error;
            } else {
                return HistoryStatus::Success;
            }
        }

        /// Dispatch a `VarCommand` to `handle_set` or `handle_unset`.
        ///
        /// Routes `cmd.action()` to the appropriate sub-handler. An `Unknown`
        /// action emits an `unknown_command` diagnostic.
        ///
        /// # Arguments
        ///
        /// * `cmd`    — The variable command to execute.
        /// * `ctx`    — Variable context forwarded to the sub-handler.
        /// * `config` — Configuration forwarded to `handle_set`.
        /// * `raw`    — Raw command string used for span construction in
        /// `Unknown` errors.
        /// * `sink`   — Diagnostic sink for errors and output.
        ///
        /// # Returns
        ///
        /// The `HistoryStatus` from the selected sub-handler, or
        /// `HistoryStatus::Error` for `Unknown`. Returns
        /// `HistoryStatus::Unknown` only when a new `VarCommand::Action` is
        /// added without a matching case.
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
                    bad_cmd, find_token_span(raw, bad_cmd), raw,
                    cmd.source_file(), cmd.source_line());
                sink.push(e);
                return HistoryStatus::Error;
            }
            }
            return HistoryStatus::Unknown;
        }

    } // namespace handlers
} // namespace math_solver