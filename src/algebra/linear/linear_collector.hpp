#pragma once

#include <cmath>
#include <map>
#include <set>
#include <string>

#include "ast/math/binary_expr.hpp"
#include "ast/math/expr.hpp"
#include "ast/math/expr_visitor.hpp"
#include "ast/math/number_expr.hpp"
#include "ast/math/variable_expr.hpp"
#include "diagnostics/kinds/math_errors.hpp"
#include "diagnostics/kinds/solver_errors.hpp"
#include "diagnostics/result.hpp"
#include "runtime/context/context.hpp"

namespace math_solver {

    // LinearForm represents affine expressions of the form:
    //   sum_i (coeffs[var_i] * var_i) + constant
    //
    // Invariants:
    // - coeffs only contains variables with non-negligible coefficients (see
    // simplify()).
    // - constant is always zeroed if sufficiently close to zero.
    //
    // Used as an intermediate representation for extracting linear structure
    // from ASTs.
    struct LinearForm {
        std::map<std::string, double> coeffs;
        double                        constant = 0.0;

        LinearForm()                           = default;

        LinearForm(double c) : constant(c) {}

        LinearForm(const std::string& var, double coeff = 1.0) {
            coeffs[var] = coeff;
        }

        double get_coeff(const std::string& var) const {
            auto it = coeffs.find(var);
            return it != coeffs.end() ? it->second : 0.0;
        }

        std::set<std::string> variables() const {
            std::set<std::string> vars;
            for (const auto& pair : coeffs) {
                if (std::abs(pair.second) > 1e-12) {
                    vars.insert(pair.first);
                }
            }
            return vars;
        }

        // Returns true if the form is a constant (no variables with significant
        // coefficients).
        bool       is_constant() const { return variables().empty(); }

        // Addition and subtraction are defined pointwise on coefficients and
        // constants.
        LinearForm operator+(const LinearForm& other) const {
            LinearForm result = *this;
            result.constant += other.constant;
            for (const auto& pair : other.coeffs) {
                result.coeffs[pair.first] += pair.second;
            }
            return result;
        }

        LinearForm operator-(const LinearForm& other) const {
            LinearForm result = *this;
            result.constant -= other.constant;
            for (const auto& pair : other.coeffs) {
                result.coeffs[pair.first] -= pair.second;
            }
            return result;
        }

        // Scalar multiplication is only valid for real scalars.
        LinearForm operator*(double scalar) const {
            LinearForm result;
            result.constant = constant * scalar;
            for (const auto& pair : coeffs) {
                result.coeffs[pair.first] = pair.second * scalar;
            }
            return result;
        }

        LinearForm operator-() const { return (*this) * (-1.0); }

        // Prunes coefficients and constant that are numerically insignificant.
        // This is necessary to avoid spurious variables due to floating-point
        // error.
        void       simplify(double epsilon = 1e-12) {
            for (auto it = coeffs.begin(); it != coeffs.end();) {
                if (std::abs(it->second) < epsilon) {
                    it = coeffs.erase(it);
                } else {
                    ++it;
                }
            }
            if (std::abs(constant) < epsilon) {
                constant = 0.0;
            }
        }
    };

    // LinearCollector traverses an Expr AST and attempts to extract a
    // LinearForm.
    //
    // Correctness:
    // - Throws NonLinearError if a non-linear term is encountered (e.g., var *
    // var, var^n for n != 1).
    // - Substitutes variables from context unless isolated_ is set.
    // - Tracks variables that shadow context bindings when isolated_ is true.
    //
    // Performance:
    // - Recursively traverses the AST; substitution from context may cause deep
    // recursion.
    // - No memoization of context lookups; repeated variables may be
    // recomputed.
    //
    // Subtlety:
    // - Division and exponentiation are only allowed if the divisor/exponent is
    // constant.
    // - Floating-point comparisons use epsilon to avoid false negatives due to
    // rounding.
    class LinearCollector : public ExprVisitor {
        private:
        LinearForm                result_;
        std::optional<Diagnostic> error_;
        const Context*            context_;
        std::string               input_;
        bool                      isolated_;

        std::set<std::string>     shadowed_vars_;

        public:
        LinearCollector() : context_(nullptr), input_(), isolated_(false) {}

        explicit LinearCollector(const Context* ctx, bool isolated = false)
            : context_(ctx), input_(), isolated_(isolated) {}

        LinearCollector(const Context* ctx, const std::string& input,
                        bool isolated = false)
            : context_(ctx), input_(input), isolated_(isolated) {}

        void set_input(const std::string& input) { input_ = input; }
        void set_isolated(bool isolated) { isolated_ = isolated; }

        // Entry point: collects a LinearForm from the given expression.
        // Resets internal state for each call.
        Result<LinearForm> collect(const Expr& expr) {
            result_ = LinearForm();
            error_.reset();
            shadowed_vars_.clear();
            expr.accept(*this);
            if (error_)
                return Result<LinearForm>::err(*error_);
            result_.simplify();
            return Result<LinearForm>::ok(result_);
        }

        // Returns variables that were present in the context but not
        // substituted due to isolation.
        const std::set<std::string>& shadowed_variables() const {
            return shadowed_vars_;
        }

        void visit(const Number& node) override {
            result_ = LinearForm(node.value());
        }

        void visit(const Variable& node) override {
            const std::string& name = node.name();

            // Substitution from context is only performed if not isolated.
            // If isolated, variables that shadow context bindings are tracked
            // for diagnostics.
            if (context_ && context_->has(name) && !isolated_) {
                const Expr& stored = context_->get_expr(name);
                stored.accept(*this);
                return;
            }

            if (context_ && context_->has(name) && isolated_) {
                shadowed_vars_.insert(name);
            }

            result_ = LinearForm(name, 1.0);
        }

        void visit(const BinaryOp& node) override {
            if (error_)
                return;
            node.left().accept(*this);
            LinearForm left = result_;

            if (error_)
                return;

            node.right().accept(*this);
            LinearForm right = result_;

            if (error_)
                return;

            switch (node.op()) {
            case BinaryOpType::Add:
                result_ = left + right;
                break;

            case BinaryOpType::Sub:
                result_ = left - right;
                break;

            case BinaryOpType::Mul:
                // Only allow multiplication if at least one operand is
                // constant. Otherwise, the term is non-linear and must be
                // rejected.
                if (left.is_constant()) {
                    result_ = right * left.constant;
                } else if (right.is_constant()) {
                    result_ = left * right.constant;
                } else {
                    error_ = errors::non_linear(
                        "non-linear term: variables multiplied together",
                        node.span(), input_);
                    return;
                }
                break;

            case BinaryOpType::Div:
                // Division is only linear if the divisor is constant and
                // nonzero.
                if (!right.is_constant()) {
                    error_ = errors::non_linear(
                        "non-linear term: division by variable", node.span(),
                        input_);
                    return;
                }
                if (std::abs(right.constant) < 1e-12) {
                    error_ = errors::math("division by zero",
                                          node.right().span(), input_);
                    return;
                }
                result_ = left * (1.0 / right.constant);
                break;

            case BinaryOpType::Pow:
                // Exponentiation is only linear if the exponent is constant and
                // equals 1.
                if (!right.is_constant()) {
                    error_ =
                        errors::non_linear("non-linear term: variable exponent",
                                           node.right().span(), input_);
                    return;
                }

                double exp = right.constant;

                // x^0 is always 1, regardless of x.
                if (std::abs(exp) < 1e-12) {
                    result_ = LinearForm(1.0);
                    break;
                }

                // x^1 is linear in x.
                if (std::abs(exp - 1.0) < 1e-12) {
                    result_ = left;
                    break;
                }

                // For all other exponents, only allow if base is constant.
                if (!left.is_constant()) {
                    error_ = errors::non_linear(
                        "non-linear term: variable raised to power " +
                            std::to_string(static_cast<int>(exp)),
                        node.span(), input_);
                    return;
                }

                result_ = LinearForm(std::pow(left.constant, exp));
                break;
            }
        }
    };

} // namespace math_solver
