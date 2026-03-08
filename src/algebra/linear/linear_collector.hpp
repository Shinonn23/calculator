#pragma once

//! # Module — `src/algebra/linear/linear_collector.hpp`
//!
//! Defines `LinearForm` — an affine intermediate representation — and
//! `LinearCollector` — an AST visitor that extracts a `LinearForm` from a
//! math expression tree, rejecting any non-linear construct with a diagnostic.
//!
//! Part of the algebra layer; sits between the math parser and the linear
//! equation solver. The collector is context-aware: it substitutes variables
//! from a `Context` by default, and can optionally operate in isolated mode
//! (treating context-shadowed names as free variables).

#include "core/tolerance.hpp"
#include <cmath>
#include <map>
#include <set>
#include <string>

#include "ast/math/array_expr.hpp"
#include "ast/math/binary_expr.hpp"
#include "ast/math/call_expr.hpp"
#include "ast/math/expr.hpp"
#include "ast/math/expr_visitor.hpp"
#include "ast/math/number_expr.hpp"
#include "ast/math/unary_expr.hpp"
#include "ast/math/variable_expr.hpp"
#include "diagnostics/kinds/math_errors.hpp"
#include "diagnostics/kinds/solver_errors.hpp"
#include "diagnostics/result.hpp"
#include "runtime/context/context.hpp"

namespace math_solver {

    /// Affine expression of the form `∑ᵢ coeffs[xᵢ]·xᵢ + constant`.
    ///
    /// Used as an intermediate representation when collecting linear structure
    /// from an AST. Coefficients with absolute value below `kEpsilon` are
    /// considered zero and are pruned by `simplify()`.
    struct LinearForm {
        /// Map of variable name to its linear coefficient.
        std::map<std::string, double> coeffs;

        /// The constant (bias) term.
        double                        constant = 0.0;

        /// Constructs the zero linear form.
        LinearForm()                           = default;

        /// Constructs the constant linear form `c`.
        LinearForm(double c) : constant(c) {}

        /// Constructs the linear form `coeff * var`.
        ///
        /// # Arguments
        ///
        /// * `var`   — Variable name.
        /// * `coeff` — Coefficient of the variable (default 1.0).
        LinearForm(const std::string& var, double coeff = 1.0) {
            coeffs[var] = coeff;
        }

        /// Returns the coefficient of `var`, or `0.0` if not present.
        double get_coeff(const std::string& var) const {
            auto it = coeffs.find(var);
            return it != coeffs.end() ? it->second : 0.0;
        }

        /// Returns the set of variable names with non-negligible coefficients.
        std::set<std::string> variables() const {
            std::set<std::string> vars;
            for (const auto& pair : coeffs) {
                if (std::abs(pair.second) > kEpsilon) {
                    vars.insert(pair.first);
                }
            }
            return vars;
        }

        /// Returns `true` if no variable has a coefficient above `kEpsilon`.
        bool       is_constant() const { return variables().empty(); }

        /// Returns the pointwise sum of two linear forms.
        LinearForm operator+(const LinearForm& other) const {
            LinearForm result = *this;
            result.constant += other.constant;
            for (const auto& pair : other.coeffs) {
                result.coeffs[pair.first] += pair.second;
            }
            return result;
        }

        /// Returns the pointwise difference of two linear forms.
        LinearForm operator-(const LinearForm& other) const {
            LinearForm result = *this;
            result.constant -= other.constant;
            for (const auto& pair : other.coeffs) {
                result.coeffs[pair.first] -= pair.second;
            }
            return result;
        }

        /// Returns this linear form scaled by `scalar`.
        LinearForm operator*(double scalar) const {
            LinearForm result;
            result.constant = constant * scalar;
            for (const auto& pair : coeffs) {
                result.coeffs[pair.first] = pair.second * scalar;
            }
            return result;
        }

        /// Returns the negation of this linear form.
        LinearForm operator-() const { return (*this) * (-1.0); }

        /// Prunes numerically insignificant coefficients and the constant term.
        ///
        /// Entries whose absolute value is below `epsilon` are removed. This
        /// prevents spurious variable entries caused by floating-point
        /// cancellation.
        ///
        /// # Arguments
        ///
        /// * `epsilon` — Threshold below which a value is considered zero
        ///   (defaults to `kEpsilon`).
        void       simplify(double epsilon = kEpsilon) {
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

    /// AST visitor that extracts a `LinearForm` from a math expression.
    ///
    /// Traverses the expression tree and accumulates a `LinearForm`.
    /// Non-linear constructs (variable × variable, variable ^ n for n ≠ 1,
    /// division by a variable, function applied to a variable) are rejected
    /// with a `Diagnostic`.
    ///
    /// When a `Context` is supplied and `isolated` is `false` (the default),
    /// variables that are bound in the context are substituted recursively.
    /// In isolated mode the context is ignored for collection, and any
    /// variable that shadows a context binding is recorded in
    /// `shadowed_variables()` for diagnostic purposes.
    class LinearCollector : public ExprVisitor {
        private:
        LinearForm                result_;
        std::optional<Diagnostic> error_;
        const Context*            context_;
        std::string               input_;
        bool                      isolated_;

        std::set<std::string>     shadowed_vars_;

        public:
        /// Constructs a collector with no context and no source string.
        LinearCollector() : context_(nullptr), input_(), isolated_(false) {}

        /// Constructs a collector with a context and optional isolation flag.
        ///
        /// # Arguments
        ///
        /// * `ctx`      — Context used for variable substitution; may be null.
        /// * `isolated` — If `true`, context bindings are not substituted.
        explicit LinearCollector(const Context* ctx, bool isolated = false)
            : context_(ctx), input_(), isolated_(isolated) {}

        /// Constructs a collector with a context, source string, and isolation
        /// flag.
        ///
        /// # Arguments
        ///
        /// * `ctx`      — Context used for variable substitution; may be null.
        /// * `input`    — Raw source text used for diagnostic span labelling.
        /// * `isolated` — If `true`, context bindings are not substituted.
        LinearCollector(const Context* ctx, const std::string& input,
                        bool isolated = false)
            : context_(ctx), input_(input), isolated_(isolated) {}

        /// Sets the raw source string used in diagnostic messages.
        void set_input(const std::string& input) { input_ = input; }

        /// Enables or disables isolated mode (no context substitution).
        void set_isolated(bool isolated) { isolated_ = isolated; }

        /// Collects a `LinearForm` from `expr`.
        ///
        /// Resets internal state (result, error, shadowed variables) before
        /// traversal, so this method may be called multiple times on the same
        /// collector instance.
        ///
        /// # Arguments
        ///
        /// * `expr` — Root of the math expression AST to collect from.
        ///
        /// # Returns
        ///
        /// The collected `LinearForm` (with `simplify()` applied) on success.
        ///
        /// # Errors
        ///
        /// Returns a `Diagnostic` when the expression contains any of:
        /// - An `ArrayExpr` node (invalid equation, no error code exposed here).
        /// - A non-linear multiplication (`var * var`).
        /// - A non-linear division (divisor contains a variable).
        /// - A non-linear exponent (variable base raised to power ≠ 1).
        /// - A function applied to a variable expression.
        /// - Division by zero.
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

        /// Returns the set of variable names that shadow a context binding
        /// when running in isolated mode.
        ///
        /// Only populated after a call to `collect` with `isolated_ == true`.
        const std::set<std::string>& shadowed_variables() const {
            return shadowed_vars_;
        }

        /// Reject an `ArrayExpr` node with an invalid-equation diagnostic.
        void visit(const ArrayExpr& node) override {
            error_ = errors::invalid_equation(
                "array value cannot appear in a linear equation",
                node.span(), input_);
        }

        /// Store the constant linear form `value` from a `Number` leaf.
        void visit(const Number& node) override {
            result_ = LinearForm(node.value());
        }

        /// Record or substitute a `Variable` node.
        ///
        /// In non-isolated mode, variables bound in `context_` are resolved
        /// by recursively visiting their stored expression. In isolated mode,
        /// context-bound names are treated as free unknowns and recorded in
        /// `shadowed_vars_` for diagnostic purposes.
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

        /// Negate the accumulated linear form for a `UnaryOp` node.
        void visit(const UnaryOp& node) override {
            if (error_)
                return;
            node.operand().accept(*this);
            if (error_)
                return;
            switch (node.op()) {
            case UnaryOpType::Neg:
                result_ = -result_;
                break;
            }
        }

        /// Combine left and right linear forms for a `BinaryOp` node.
        ///
        /// Rejects multiplication of two variable-containing forms, division
        /// by a variable expression, and any exponent that is neither 0 nor 1
        /// on a non-constant base. Sets `error_` and aborts on any non-linear
        /// term.
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
                    error_ = errors::unsupported_equation(
                        "non-linear term: variables multiplied together",
                        node.span(), input_);
                    return;
                }
                break;

            case BinaryOpType::Div:
                // Division is only linear if the divisor is constant and
                // nonzero.
                if (!right.is_constant()) {
                    error_ = errors::unsupported_equation(
                        "non-linear term: division by variable", node.span(),
                        input_);
                    return;
                }
                if (std::abs(right.constant) < kEpsilon) {
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
                        errors::unsupported_equation("non-linear term: variable exponent",
                                           node.right().span(), input_);
                    return;
                }

                double exp = right.constant;

                // x^0 is always 1, regardless of x.
                if (std::abs(exp) < kEpsilon) {
                    result_ = LinearForm(1.0);
                    break;
                }

                // x^1 is linear in x.
                if (std::abs(exp - 1.0) < kEpsilon) {
                    result_ = left;
                    break;
                }

                // For all other exponents, only allow if base is constant.
                if (!left.is_constant()) {
                    error_ = errors::unsupported_equation(
                        "non-linear term: variable raised to power " +
                            std::to_string(static_cast<int>(exp)),
                        node.span(), input_);
                    return;
                }

                result_ = LinearForm(std::pow(left.constant, exp));
                break;
            }
        }

        /// Evaluate a `FunctionCall` numerically if its argument is constant.
        ///
        /// If the argument contains a variable, sets `error_` with an
        /// unsupported-equation diagnostic and aborts traversal.
        void visit(const FunctionCall& node) override {
            if (error_)
                return;
            // Recurse into the argument to determine if it is constant.
            node.arg().accept(*this);
            if (error_)
                return;
            LinearForm arg_form = result_;

            if (!arg_form.is_constant()) {
                // Function applied to a variable expression — non-linear.
                error_ = errors::unsupported_equation(
                    "non-linear term: function '" + node.name() +
                        "' applied to variable expression",
                    node.span(), input_);
                return;
            }

            // Constant argument — evaluate numerically.
            double x = arg_form.constant;
            double val = 0.0;
            switch (node.kind()) {
            case FuncKind::Sin:   val = std::sin(x);   break;
            case FuncKind::Cos:   val = std::cos(x);   break;
            case FuncKind::Tan:   val = std::tan(x);   break;
            case FuncKind::Asin:  val = std::asin(x);  break;
            case FuncKind::Acos:  val = std::acos(x);  break;
            case FuncKind::Atan:  val = std::atan(x);  break;
            case FuncKind::Sinh:  val = std::sinh(x);  break;
            case FuncKind::Cosh:  val = std::cosh(x);  break;
            case FuncKind::Tanh:  val = std::tanh(x);  break;
            case FuncKind::Exp:   val = std::exp(x);   break;
            case FuncKind::Sqrt:  val = std::sqrt(x);  break;
            case FuncKind::Ln:    val = std::log(x);   break;
            case FuncKind::Log:   val = std::log10(x); break;
            case FuncKind::Abs:   val = std::abs(x);   break;
            case FuncKind::Floor: val = std::floor(x); break;
            case FuncKind::Ceil:  val = std::ceil(x);  break;
            case FuncKind::Round: val = std::round(x); break;
            }
            result_ = LinearForm(val);
        }
    };

} // namespace math_solver
