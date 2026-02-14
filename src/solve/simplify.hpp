#ifndef SIMPLIFY_H
#define SIMPLIFY_H

#include "../ast/equation.hpp"
#include "../ast/expr.hpp"
#include "../common/fraction.hpp"
#include "../eval/context.hpp"
#include "linear_collector.hpp"
#include <algorithm>
#include <cmath>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace math_solver {

    // Options for simplification
    struct SimplifyOptions {
        std::vector<std::string> var_order; // Order of variables (empty = auto)
        bool                     isolated;  // Ignore context
        bool as_fraction;                   // Display coefficients as fractions
        bool show_zero_coeffs;              // Show 0x terms

        SimplifyOptions()
            : isolated(false), as_fraction(false), show_zero_coeffs(false) {}
    };

    // Result of simplification
    struct SimplifyResult {
        LinearForm               form;      // Canonical linear form
        std::vector<std::string> var_order; // Variable ordering used
        std::string              canonical; // Formatted string
        std::set<std::string>    warnings;  // Any warnings

        bool                     is_no_solution() const {
            // 0 = c where c != 0
            return form.is_constant() && std::abs(form.constant) > 1e-12;
        }

        bool is_infinite_solutions() const {
            // 0 = 0
            return form.is_constant() && std::abs(form.constant) < 1e-12;
        }
    };

    // Simplifies equations to canonical linear form
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

        // Simplify an equation to canonical form: Ax + By + Cz = D
        SimplifyResult
        simplify(const Equation&        eq,
                 const SimplifyOptions& opts = SimplifyOptions()) {
            SimplifyResult result;

            // First pass: collect all variables without substitution
            // to detect shadowing
            if (context_ && !opts.isolated) {
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
            }

            // Collect linear forms (with or without context)
            LinearCollector collector(opts.isolated ? nullptr : context_,
                                      input_,
                                      false // not isolated for collector itself
            );

            LinearForm lhs        = collector.collect(eq.lhs());
            LinearForm rhs        = collector.collect(eq.rhs());

            // Normalize: lhs - rhs = 0, so lhs = rhs becomes coeffs = -constant
            LinearForm normalized = lhs - rhs;
            normalized.simplify();

            // Store result form (variables on left, constant on right)
            result.form = normalized;

            // Determine variable order
            if (!opts.var_order.empty()) {
                result.var_order = opts.var_order;
            } else {
                // Auto-detect: alphabetical order
                auto vars = normalized.variables();
                result.var_order =
                    std::vector<std::string>(vars.begin(), vars.end());
                std::sort(result.var_order.begin(), result.var_order.end());
            }

            // Format canonical string
            result.canonical =
                format_canonical(normalized, result.var_order, opts);

            return result;
        }

        // Simplify a single expression (not an equation)
        SimplifyResult
        simplify_expr(const Expr&            expr,
                      const SimplifyOptions& opts = SimplifyOptions()) {
            SimplifyResult  result;

            LinearCollector collector(opts.isolated ? nullptr : context_,
                                      input_, opts.isolated);

            LinearForm form = collector.collect(expr);
            form.simplify();

            result.form = form;

            // Determine variable order
            if (!opts.var_order.empty()) {
                result.var_order = opts.var_order;
            } else {
                auto vars = form.variables();
                result.var_order =
                    std::vector<std::string>(vars.begin(), vars.end());
                std::sort(result.var_order.begin(), result.var_order.end());
            }

            // Format as expression (not equation)
            result.canonical = format_expression(form, result.var_order, opts);

            return result;
        }

        private:
        // Format as: Ax + By + Cz = D
        std::string format_canonical(const LinearForm&               form,
                                     const std::vector<std::string>& var_order,
                                     const SimplifyOptions&          opts) {
            std::ostringstream oss;

            bool               first = true;

            // Left side: all variable terms
            for (const auto& var : var_order) {
                double coeff = form.get_coeff(var);

                // Skip zero coefficients unless requested
                if (std::abs(coeff) < 1e-12 && !opts.show_zero_coeffs) {
                    continue;
                }

                // Handle sign and spacing
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

                // Format coefficient
                std::string coeff_str;
                if (opts.as_fraction) {
                    coeff_str = format_coefficient(coeff, false, true);
                } else {
                    coeff_str = format_coefficient(coeff, false, false);
                }

                // Special handling for coefficient of 0
                if (std::abs(form.get_coeff(var)) < 1e-12) {
                    oss << "0" << var;
                } else if (coeff_str.empty()) {
                    oss << var;
                } else {
                    oss << coeff_str << var;
                }

                first = false;
            }

            // If no variable terms were written
            if (first) {
                oss << "0";
            }

            // Right side: constant (negated because we have coeffs - constant =
            // 0)
            double rhs = -form.constant;

            // Fix -0 display
            if (std::abs(rhs) < 1e-12) {
                rhs = 0.0;
            }

            oss << " = ";

            if (opts.as_fraction) {
                Fraction frac = double_to_fraction(rhs);
                oss << frac.to_string();
            } else {
                // Format constant
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

        // Format as expression (no = D part)
        std::string format_expression(const LinearForm&               form,
                                      const std::vector<std::string>& var_order,
                                      const SimplifyOptions&          opts) {
            std::ostringstream oss;

            bool               first = true;

            // Variable terms
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

            // Constant term
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
