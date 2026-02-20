#ifndef SIMPLIFY_H
#define SIMPLIFY_H

#include "algebra/linear/linear_collector.hpp"
#include "ast/math/equation_expr.hpp"
#include "ast/math/expr.hpp"
#include "core/fraction.hpp"
#include "runtime/context/context.hpp"
#include <algorithm>
#include <cmath>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace math_solver {

    struct SimplifyOptions {
        std::vector<std::string> var_order;
        bool                     isolated;
        bool                     as_fraction;
        bool                     show_zero_coeffs;

        SimplifyOptions()
            : isolated(false), as_fraction(false), show_zero_coeffs(false) {}
    };

    struct SimplifyResult {
        LinearForm               form;
        std::vector<std::string> var_order;
        std::string              canonical;
        std::set<std::string>    warnings;

        // Returns true if the equation is unsatisfiable (0 = c, c != 0).
        bool                     is_no_solution() const {
            return form.is_constant() && std::abs(form.constant) > 1e-12;
        }

        // Returns true if the equation is tautological (0 = 0).
        bool is_infinite_solutions() const {
            return form.is_constant() && std::abs(form.constant) < 1e-12;
        }
    };

    // Entry point for canonicalization and normalization of linear equations
    // and expressions.
    // - Handles context-aware variable resolution and shadowing detection.
    // - Maintains variable ordering for deterministic output.
    // - Responsible for formatting output in canonical form.
    class Simplifier {
        private:
        const Context* context_;
        std::string    input_;

        public:
        Simplifier() : context_(nullptr) {}

        explicit Simplifier(const Context* ctx) : context_(ctx) {}

        Simplifier(const Context* ctx, const std::string& input)
            : context_(ctx), input_(input) {}

        void set_input(const std::string& input) { input_ = input; }

        // Canonicalizes a linear equation to the form Ax + By + Cz = D.
        // - If context is provided and not isolated, performs a shadowing check
        // to warn
        //   about variable name collisions with context bindings. This is
        //   best-effort and may fail for non-linear expressions involving
        //   context variables.
        // - Variable ordering is either user-specified or lexicographically
        // sorted for determinism.
        // - The result is normalized such that all variable terms are on the
        // left and the constant on the right.
        SimplifyResult
        simplify(const Equation&        eq,
                 const SimplifyOptions& opts = SimplifyOptions()) {
            SimplifyResult result;

            if (context_ && !opts.isolated) {
                // Shadowing detection: warn if any variable in the equation
                // collides with a context variable. This is a best-effort pass
                // and may not catch all cases if the expression is not purely
                // linear.
                try {
                    LinearCollector shadow_check(nullptr, input_, true);
                    LinearForm      lhs_vars = shadow_check.collect(eq.lhs());
                    LinearForm      rhs_vars = shadow_check.collect(eq.rhs());
                    LinearForm      all_vars = lhs_vars - rhs_vars;

                    for (const auto& var : all_vars.variables()) {
                        if (context_->has(var)) {
                            result.warnings.insert(
                                "'" + var +
                                "' in expression shadows context variable "
                                "(use --isolated to keep as variable)");
                        }
                    }
                } catch (...) {
                    // Shadowing check is intentionally best-effort and may fail
                    // for non-linear or context-dependent expressions.
                }
            }

            // Main collection pass: context is used unless isolated is
            // requested.
            LinearCollector collector(
                opts.isolated ? nullptr : context_, input_, false);

            LinearForm lhs        = collector.collect(eq.lhs());
            LinearForm rhs        = collector.collect(eq.rhs());

            // Normalize: move all terms to the left, so the equation is of the
            // form (lhs - rhs) = 0. This ensures canonicalization for
            // downstream consumers.
            LinearForm normalized = lhs - rhs;
            normalized.simplify();

            result.form = normalized;

            // Variable ordering: deterministic output is required for
            // reproducibility.
            if (!opts.var_order.empty()) {
                result.var_order = opts.var_order;
            } else {
                auto vars = normalized.variables();
                result.var_order =
                    std::vector<std::string>(vars.begin(), vars.end());
                std::sort(result.var_order.begin(), result.var_order.end());
            }

            result.canonical =
                format_canonical(normalized, result.var_order, opts);

            return result;
        }

        // Canonicalizes a single linear expression (not an equation).
        // - Variable ordering and formatting logic mirrors that of equations.
        // - Constant terms are preserved in the output.
        SimplifyResult
        simplify_expr(const Expr&            expr,
                      const SimplifyOptions& opts = SimplifyOptions()) {
            SimplifyResult  result;

            LinearCollector collector(
                opts.isolated ? nullptr : context_, input_, opts.isolated);

            LinearForm form = collector.collect(expr);
            form.simplify();

            result.form = form;

            if (!opts.var_order.empty()) {
                result.var_order = opts.var_order;
            } else {
                auto vars = form.variables();
                result.var_order =
                    std::vector<std::string>(vars.begin(), vars.end());
                std::sort(result.var_order.begin(), result.var_order.end());
            }

            result.canonical = format_expression(form, result.var_order, opts);

            return result;
        }

        private:
        // Formats a normalized linear form as "Ax + By + Cz = D".
        // - Variable terms are ordered as specified.
        // - Zero coefficients are omitted unless explicitly requested.
        // - Coefficient formatting (fractional/decimal) is controlled by
        // options.
        // - The right-hand side constant is always negated to match the
        // canonical form.
        std::string format_canonical(const LinearForm&               form,
                                     const std::vector<std::string>& var_order,
                                     const SimplifyOptions&          opts) {
            std::ostringstream oss;

            bool               first = true;

            for (const auto& var : var_order) {
                double coeff = form.get_coeff(var);

                if (std::abs(coeff) < 1e-12 && !opts.show_zero_coeffs) {
                    continue;
                }

                if (!first) {
                    if (coeff >= 0) {
                        oss << " + ";
                    } else {
                        oss << " - ";
                        coeff = -coeff;
                    }
                } else {
                    if (coeff < 0) {
                        oss << "-";
                        coeff = -coeff;
                    }
                }

                std::string coeff_str;
                if (opts.as_fraction) {
                    coeff_str = format_coefficient(coeff, false, true);
                } else {
                    coeff_str = format_coefficient(coeff, false, false);
                }

                if (std::abs(form.get_coeff(var)) < 1e-12) {
                    oss << "0" << var;
                } else if (coeff_str.empty()) {
                    oss << var;
                } else {
                    oss << coeff_str << var;
                }

                first = false;
            }

            if (first) {
                oss << "0";
            }

            double rhs = -form.constant;

            // Avoid negative zero in output.
            if (std::abs(rhs) < 1e-12) {
                rhs = 0.0;
            }

            oss << " = ";

            if (opts.as_fraction) {
                Fraction frac = double_to_fraction(rhs);
                oss << frac.to_string();
            } else {
                std::string rhs_str = std::to_string(rhs);
                size_t      dot_pos = rhs_str.find('.');
                if (dot_pos != std::string::npos) {
                    rhs_str.erase(rhs_str.find_last_not_of('0') + 1);
                    if (rhs_str.back() == '.') {
                        rhs_str.pop_back();
                    }
                }
                oss << rhs_str;
            }

            return oss.str();
        }

        // Formats a linear expression as "Ax + By + C".
        // - Variable ordering and coefficient formatting mirror
        // format_canonical.
        // - Constant term is always included if nonzero or if there are no
        // variable terms.
        std::string format_expression(const LinearForm&               form,
                                      const std::vector<std::string>& var_order,
                                      const SimplifyOptions&          opts) {
            std::ostringstream oss;

            bool               first = true;

            for (const auto& var : var_order) {
                double coeff = form.get_coeff(var);

                if (std::abs(coeff) < 1e-12) {
                    continue;
                }

                if (!first) {
                    if (coeff >= 0) {
                        oss << " + ";
                    } else {
                        oss << " - ";
                        coeff = -coeff;
                    }
                } else {
                    if (coeff < 0) {
                        oss << "-";
                        coeff = -coeff;
                    }
                }

                std::string coeff_str;
                if (opts.as_fraction) {
                    coeff_str = format_coefficient(coeff, false, true);
                } else {
                    coeff_str = format_coefficient(coeff, false, false);
                }

                if (coeff_str.empty()) {
                    oss << var;
                } else {
                    oss << coeff_str << var;
                }

                first = false;
            }

            // Constant term: always included if nonzero or if there are no
            // variable terms.
            if (std::abs(form.constant) > 1e-12 || first) {
                double c = form.constant;
                if (!first) {
                    if (c >= 0) {
                        oss << " + ";
                    } else {
                        oss << " - ";
                        c = -c;
                    }
                } else if (c < 0) {
                    oss << "-";
                    c = -c;
                }

                if (opts.as_fraction) {
                    Fraction frac = double_to_fraction(c);
                    oss << frac.to_string();
                } else {
                    std::string c_str   = std::to_string(c);
                    size_t      dot_pos = c_str.find('.');
                    if (dot_pos != std::string::npos) {
                        c_str.erase(c_str.find_last_not_of('0') + 1);
                        if (c_str.back() == '.') {
                            c_str.pop_back();
                        }
                    }
                    oss << c_str;
                }
            }

            return oss.str();
        }
    };

} // namespace math_solver

#endif
