#ifndef QUADRATIC_COLLECTOR_H
#define QUADRATIC_COLLECTOR_H

#include "ast/binary.h"
#include "ast/expr.h"
#include "ast/function_call.h"
#include "ast/index_access.h"
#include "ast/number.h"
#include "ast/number_array.h"
#include "ast/substitutor.h"
#include "ast/variable.h"
#include "common/error.h"
#include "eval/context.h"
#include <cmath>
#include <map>
#include <set>
#include <string>

namespace math_solver {

    // Represents a quadratic form: ax^2 + bx + c (single variable) or
    // sum of linear terms plus one quadratic term.
    // Stores: quadratic_coeffs (var -> coeff of var^2),
    //         linear_coeffs (var -> coeff of var), constant
    struct QuadraticForm {
        std::map<std::string, double> quadratic_coeffs; // var -> coeff of var^2
        std::map<std::string, double> linear_coeffs;    // var -> coeff of var
        double                        constant = 0.0;

        QuadraticForm()                        = default;

        explicit QuadraticForm(double c) : constant(c) {}

        // Construct as a linear variable term
        static QuadraticForm linear(const std::string& var,
                                    double             coeff = 1.0) {
            QuadraticForm f;
            f.linear_coeffs[var] = coeff;
            return f;
        }

        // Construct as a quadratic variable term
        static QuadraticForm quadratic(const std::string& var,
                                       double             coeff = 1.0) {
            QuadraticForm f;
            f.quadratic_coeffs[var] = coeff;
            return f;
        }

        double get_quad_coeff(const std::string& var) const {
            auto it = quadratic_coeffs.find(var);
            return it != quadratic_coeffs.end() ? it->second : 0.0;
        }

        double get_linear_coeff(const std::string& var) const {
            auto it = linear_coeffs.find(var);
            return it != linear_coeffs.end() ? it->second : 0.0;
        }

        bool is_constant() const {
            return quad_variables().empty() && linear_variables().empty();
        }

        bool is_linear() const { return quad_variables().empty(); }

        std::set<std::string> quad_variables() const {
            std::set<std::string> vars;
            for (const auto& p : quadratic_coeffs) {
                if (std::abs(p.second) > 1e-12)
                    vars.insert(p.first);
            }
            return vars;
        }

        std::set<std::string> linear_variables() const {
            std::set<std::string> vars;
            for (const auto& p : linear_coeffs) {
                if (std::abs(p.second) > 1e-12)
                    vars.insert(p.first);
            }
            return vars;
        }

        std::set<std::string> all_variables() const {
            auto vars = quad_variables();
            auto lin  = linear_variables();
            vars.insert(lin.begin(), lin.end());
            return vars;
        }

        QuadraticForm operator+(const QuadraticForm& other) const {
            QuadraticForm result = *this;
            result.constant += other.constant;
            for (const auto& p : other.linear_coeffs)
                result.linear_coeffs[p.first] += p.second;
            for (const auto& p : other.quadratic_coeffs)
                result.quadratic_coeffs[p.first] += p.second;
            return result;
        }

        QuadraticForm operator-(const QuadraticForm& other) const {
            QuadraticForm result = *this;
            result.constant -= other.constant;
            for (const auto& p : other.linear_coeffs)
                result.linear_coeffs[p.first] -= p.second;
            for (const auto& p : other.quadratic_coeffs)
                result.quadratic_coeffs[p.first] -= p.second;
            return result;
        }

        QuadraticForm operator*(double scalar) const {
            QuadraticForm result;
            result.constant = constant * scalar;
            for (const auto& p : linear_coeffs)
                result.linear_coeffs[p.first] = p.second * scalar;
            for (const auto& p : quadratic_coeffs)
                result.quadratic_coeffs[p.first] = p.second * scalar;
            return result;
        }

        QuadraticForm operator-() const { return (*this) * (-1.0); }

        void          simplify(double epsilon = 1e-12) {
            for (auto it = linear_coeffs.begin(); it != linear_coeffs.end();) {
                if (std::abs(it->second) < epsilon)
                    it = linear_coeffs.erase(it);
                else
                    ++it;
            }
            for (auto it = quadratic_coeffs.begin();
                 it != quadratic_coeffs.end();) {
                if (std::abs(it->second) < epsilon)
                    it = quadratic_coeffs.erase(it);
                else
                    ++it;
            }
            if (std::abs(constant) < epsilon)
                constant = 0.0;
        }
    };

    // Visitor that collects quadratic coefficients from an expression.
    // Supports linear + single-variable quadratic terms.
    // Throws for terms it cannot handle (e.g., x*y, x^3, x^y).
    class QuadraticCollector : public ExprVisitor {
        private:
        QuadraticForm  result_;
        const Context* context_;
        std::string    input_;

        public:
        QuadraticCollector() : context_(nullptr) {}

        explicit QuadraticCollector(const Context*     ctx,
                                    const std::string& input = "")
            : context_(ctx), input_(input) {}

        void          set_input(const std::string& input) { input_ = input; }

        QuadraticForm collect(const Expr& expr) {
            result_ = QuadraticForm();
            expr.accept(*this);
            result_.simplify();
            return result_;
        }

        void visit(const Number& node) override {
            result_ = QuadraticForm(node.value());
        }

        void visit(const Variable& node) override {
            const std::string& name = node.name();

            // Substitute from context if available
            if (context_ && context_->has(name)) {
                const Expr* stored = context_->get_expr(name);
                if (stored) {
                    // Only catch circular-reference errors from expand();
                    // let NonLinearError from accept() propagate naturally.
                    ExprPtr expanded;
                    try {
                        expanded = context_->expand(*stored);
                    } catch (const std::runtime_error&) {
                        // Expansion failed (e.g., circular ref)
                        expanded = nullptr;
                    }
                    if (expanded) {
                        expanded->accept(*this);
                        return;
                    }
                }
            }

            result_ = QuadraticForm::linear(name, 1.0);
        }

        void visit(const FunctionCall& node) override {
            // Evaluate each argument — if all are constant, compute result
            std::vector<double> arg_values;
            bool                all_constant = true;

            for (size_t i = 0; i < node.arg_count(); ++i) {
                node.arg(i).accept(*this);
                if (!result_.is_constant()) {
                    all_constant = false;
                    break;
                }
                arg_values.push_back(result_.constant);
            }

            if (all_constant && arg_values.size() == 1) {
                double             x    = arg_values[0];
                double             val  = 0.0;
                const std::string& name = node.name();

                if (name == "sqrt") {
                    if (x < 0)
                        throw MathError("sqrt of negative number", node.span(),
                                        input_);
                    val = std::sqrt(x);
                } else if (name == "abs") {
                    val = std::abs(x);
                } else if (name == "sin") {
                    val = std::sin(x);
                } else if (name == "cos") {
                    val = std::cos(x);
                } else if (name == "tan") {
                    val = std::tan(x);
                } else if (name == "log") {
                    if (x <= 0)
                        throw MathError("log of non-positive number",
                                        node.span(), input_);
                    val = std::log10(x);
                } else if (name == "ln") {
                    if (x <= 0)
                        throw MathError("ln of non-positive number",
                                        node.span(), input_);
                    val = std::log(x);
                } else if (name == "exp") {
                    val = std::exp(x);
                } else if (name == "floor") {
                    val = std::floor(x);
                } else if (name == "ceil") {
                    val = std::ceil(x);
                } else {
                    throw NonLinearError("unknown function '" + name + "'",
                                         node.span(), input_);
                }
                result_ = QuadraticForm(val);
            } else {
                // Function of variable — non-linear
                throw NonLinearError("non-linear term: function '" +
                                         node.name() +
                                         "' applied to variable expression",
                                     node.span(), input_);
            }
        }

        void visit(const NumberArray& node) override {
            if (node.size() == 1) {
                result_ = QuadraticForm(node.at(0));
            } else {
                throw NonLinearError("cannot use array with " +
                                         std::to_string(node.size()) +
                                         " elements in equation (use [index])",
                                     node.span(), input_);
            }
        }

        void visit(const IndexAccess& node) override {
            auto* var = dynamic_cast<const Variable*>(&node.target());
            if (var && context_ && context_->has(var->name())) {
                const Expr* stored = context_->get_expr(var->name());
                auto*       arr    = dynamic_cast<const NumberArray*>(stored);
                if (arr) {
                    if (node.index() < arr->size()) {
                        result_ = QuadraticForm(arr->at(node.index()));
                        return;
                    }
                }
                if (stored) {
                    ExprPtr expanded;
                    try {
                        expanded = context_->expand(*stored);
                    } catch (...) {
                        expanded = nullptr;
                    }
                    if (expanded) {
                        auto* earr =
                            dynamic_cast<const NumberArray*>(expanded.get());
                        if (earr && node.index() < earr->size()) {
                            result_ = QuadraticForm(earr->at(node.index()));
                            return;
                        }
                    }
                }
            }
            throw NonLinearError("cannot resolve indexed access in equation",
                                 node.span(), input_);
        }

        void visit(const BinaryOp& node) override {
            node.left().accept(*this);
            QuadraticForm left = result_;

            node.right().accept(*this);
            QuadraticForm right = result_;

            switch (node.op()) {
            case BinaryOpType::Add:
                result_ = left + right;
                break;

            case BinaryOpType::Sub:
                result_ = left - right;
                break;

            case BinaryOpType::Mul:
                if (left.is_constant()) {
                    result_ = right * left.constant;
                } else if (right.is_constant()) {
                    result_ = left * right.constant;
                } else if (left.is_linear() && right.is_linear()) {
                    // linear * linear can produce quadratic
                    // e.g., (ax + b)(cx + d) = ac*x^2 + (ad+bc)*x + bd
                    // But we only support single-variable quadratic
                    auto l_vars = left.linear_variables();
                    auto r_vars = right.linear_variables();

                    // Simple case: both are single-variable, same var
                    // or one is just a constant + variable term
                    result_     = multiply_linear(left, right, node.span());
                } else {
                    throw NonLinearError(
                        "non-linear term: higher-order multiplication",
                        node.span(), input_);
                }
                break;

            case BinaryOpType::Div:
                if (!right.is_constant()) {
                    throw NonLinearError(
                        "non-linear term: division by variable", node.span(),
                        input_);
                }
                if (std::abs(right.constant) < 1e-12) {
                    throw MathError("division by zero", node.right().span(),
                                    input_);
                }
                result_ = left * (1.0 / right.constant);
                break;

            case BinaryOpType::Pow:
                if (!right.is_constant()) {
                    throw NonLinearError("non-linear term: variable exponent",
                                         node.span(), input_);
                }

                double exp = right.constant;

                if (std::abs(exp) < 1e-12) {
                    result_ = QuadraticForm(1.0);
                    break;
                }

                if (std::abs(exp - 1.0) < 1e-12) {
                    result_ = left;
                    break;
                }

                if (std::abs(exp - 2.0) < 1e-12 && left.is_linear()) {
                    // x^2: linear form squared
                    result_ = square_linear(left, node.span());
                    break;
                }

                if (left.is_constant()) {
                    result_ = QuadraticForm(std::pow(left.constant, exp));
                    break;
                }

                throw NonLinearError(
                    "non-linear term: variable raised to power " +
                        std::to_string(static_cast<int>(exp)),
                    node.span(), input_);
            }
        }

        private:
        // Multiply two linear forms to produce a quadratic form
        QuadraticForm multiply_linear(const QuadraticForm& left,
                                      const QuadraticForm& right,
                                      const Span&          span) {
            QuadraticForm result;

            // constant * constant
            result.constant = left.constant * right.constant;

            // constant * linear terms
            for (const auto& p : right.linear_coeffs) {
                result.linear_coeffs[p.first] += left.constant * p.second;
            }
            for (const auto& p : left.linear_coeffs) {
                result.linear_coeffs[p.first] += right.constant * p.second;
            }

            // linear * linear -> quadratic
            for (const auto& lp : left.linear_coeffs) {
                for (const auto& rp : right.linear_coeffs) {
                    if (lp.first == rp.first) {
                        // same variable: x * x = x^2
                        result.quadratic_coeffs[lp.first] +=
                            lp.second * rp.second;
                    } else {
                        // different variables: x * y — not supported
                        throw NonLinearError("non-linear term: product of "
                                             "different variables (" +
                                                 lp.first + " * " + rp.first +
                                                 ")",
                                             span, input_);
                    }
                }
            }

            return result;
        }

        // Square a linear form: (ax + b)^2 = a^2 x^2 + 2ab x + b^2
        QuadraticForm square_linear(const QuadraticForm& form,
                                    const Span&          span) {
            auto vars = form.linear_variables();
            if (vars.size() > 1) {
                throw NonLinearError(
                    "non-linear term: squaring multi-variable expression", span,
                    input_);
            }

            if (vars.empty()) {
                // Constant squared
                return QuadraticForm(form.constant * form.constant);
            }

            std::string   var = *vars.begin();
            double        a   = form.get_linear_coeff(var);
            double        b   = form.constant;

            QuadraticForm result;
            result.quadratic_coeffs[var] = a * a;
            result.linear_coeffs[var]    = 2 * a * b;
            result.constant              = b * b;
            return result;
        }
    };

} // namespace math_solver

#endif
