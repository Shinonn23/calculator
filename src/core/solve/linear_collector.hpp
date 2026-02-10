#ifndef LINEAR_COLLECTOR_H
#define LINEAR_COLLECTOR_H

#include "../ast/binary.hpp"
#include "../ast/expr.hpp"
#include "../ast/number.hpp"
#include "../ast/variable.hpp"
#include "../common/error.hpp"
#include "../eval/context.hpp"
#include <cmath>
#include <map>
#include <set>
#include <string>

namespace math_solver {

    // Represents a linear form: sum of (coeff * var) + constant
    // Example: 3x + 2y - 5 is represented as coeffs={x:3, y:2}, constant=-5
    struct LinearForm {
        std::map<std::string, double> coeffs; // variable -> coefficient
        double                        constant = 0.0;

        LinearForm()                           = default;

        LinearForm(double c) : constant(c) {}

        LinearForm(const std::string& var, double coeff = 1.0) {
            coeffs[var] = coeff;
        }

        // Get coefficient for a variable (0 if not present)
        double get_coeff(const std::string& var) const {
            auto it = coeffs.find(var);
            return it != coeffs.end() ? it->second : 0.0;
        }

        // Get all variable names
        std::set<std::string> variables() const {
            std::set<std::string> vars;
            for (const auto& pair : coeffs) {
                if (std::abs(pair.second) > 1e-12) {
                    vars.insert(pair.first);
                }
            }
            return vars;
        }

        // Check if this is just a constant (no variables)
        bool       is_constant() const { return variables().empty(); }

        // Add two linear forms
        LinearForm operator+(const LinearForm& other) const {
            LinearForm result = *this;
            result.constant += other.constant;
            for (const auto& pair : other.coeffs) {
                result.coeffs[pair.first] += pair.second;
            }
            return result;
        }

        // Subtract linear forms
        LinearForm operator-(const LinearForm& other) const {
            LinearForm result = *this;
            result.constant -= other.constant;
            for (const auto& pair : other.coeffs) {
                result.coeffs[pair.first] -= pair.second;
            }
            return result;
        }

        // Multiply by a scalar
        LinearForm operator*(double scalar) const {
            LinearForm result;
            result.constant = constant * scalar;
            for (const auto& pair : coeffs) {
                result.coeffs[pair.first] = pair.second * scalar;
            }
            return result;
        }

        // Negate
        LinearForm operator-() const { return (*this) * (-1.0); }

        // Clean up near-zero coefficients
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

    // Visitor that collects linear coefficients from an expression
    // Detects non-linear terms and throws NonLinearError
    class LinearCollector : public ExprVisitor {
        private:
        LinearForm     result_;
        const Context* context_;
        std::string    input_;
        bool           isolated_; // If true, don't substitute from context

        // Track variables we've seen but haven't substituted
        std::set<std::string> shadowed_vars_;

        public:
        LinearCollector() : context_(nullptr), input_(), isolated_(false) {}

        explicit LinearCollector(const Context* ctx, bool isolated = false)
            : context_(ctx), input_(), isolated_(isolated) {}

        LinearCollector(const Context* ctx, const std::string& input,
                        bool isolated = false)
            : context_(ctx), input_(input), isolated_(isolated) {}

        void       set_input(const std::string& input) { input_ = input; }
        void       set_isolated(bool isolated) { isolated_ = isolated; }

        // Collect linear form from expression
        LinearForm collect(const Expr& expr) {
            result_ = LinearForm();
            shadowed_vars_.clear();
            expr.accept(*this);
            result_.simplify();
            return result_;
        }

        // Get variables that were in context but not substituted (shadowed)
        const std::set<std::string>& shadowed_variables() const {
            return shadowed_vars_;
        }

        void visit(const Number& node) override {
            result_ = LinearForm(node.value());
        }

        void visit(const Variable& node) override {
            const std::string& name = node.name();

            // Check if we should substitute from context
            if (context_ && context_->has(name) && !isolated_) {
                result_ = LinearForm(context_->get(name));
                return;
            }

            // Track if this variable shadows a context variable
            if (context_ && context_->has(name) && isolated_) {
                shadowed_vars_.insert(name);
            }

            // Keep as variable
            result_ = LinearForm(name, 1.0);
        }

        void visit(const BinaryOp& node) override {
            // Collect from children
            node.left().accept(*this);
            LinearForm left = result_;

            node.right().accept(*this);
            LinearForm right = result_;

            switch (node.op()) {
            case BinaryOpType::Add:
                result_ = left + right;
                break;

            case BinaryOpType::Sub:
                result_ = left - right;
                break;

            case BinaryOpType::Mul:
                // For linearity, at least one side must be constant
                if (left.is_constant()) {
                    result_ = right * left.constant;
                } else if (right.is_constant()) {
                    result_ = left * right.constant;
                } else {
                    // var * var = non-linear
                    throw NonLinearError(
                        "non-linear term: multiplication of variables",
                        node.span(), input_);
                }
                break;

            case BinaryOpType::Div:
                // For linearity, divisor must be constant
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
                // For linearity, exponent must be constant
                if (!right.is_constant()) {
                    throw NonLinearError("non-linear term: variable exponent",
                                         node.span(), input_);
                }

                double exp = right.constant;

                // x^0 = 1
                if (std::abs(exp) < 1e-12) {
                    result_ = LinearForm(1.0);
                    break;
                }

                // x^1 = x (linear)
                if (std::abs(exp - 1.0) < 1e-12) {
                    result_ = left;
                    break;
                }

                // For x^n where n != 0, 1: only valid if x is constant
                if (!left.is_constant()) {
                    throw NonLinearError(
                        "non-linear term: variable raised to power " +
                            std::to_string(static_cast<int>(exp)),
                        node.span(), input_);
                }

                result_ = LinearForm(std::pow(left.constant, exp));
                break;
            }
        }
    };

} // namespace math_solver

#endif
