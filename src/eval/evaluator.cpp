#include "evaluator.hpp"
#include "ast/math/binary_expr.hpp"
#include "ast/math/number_expr.hpp"
#include "ast/math/variable_expr.hpp"
#include "core/error.hpp"
#include <cmath>

namespace math_solver {

    void Evaluator::visit(const Number& node) {
        // Numbers are terminal nodes; evaluation is trivial.
        result_ = node.value();
    }

    void Evaluator::visit(const Variable& node) {
        // Variable resolution requires a valid context.
        if (!context_) {
            throw MathException(
                errors::undefined_variable(node.name(), node.span(), input_));
        }
        if (!context_->has(node.name())) {
            throw MathException(
                errors::undefined_variable(node.name(), node.span(), input_));
        }

        const std::string& name = node.name();

        // Detect and prevent cycles in variable definitions.
        // This is critical to avoid infinite recursion in cases like `a = a +
        // 1`.
        if (auto it = visited_.find(name); it != visited_.end()) {
            throw MathException(
                errors::circular_dependency(name, it->second, input_));
        }

        // Recursively evaluate the expression bound to this variable.
        // Note: visited_ is used as a dynamic set for the current evaluation
        // stack.
        visited_[name]     = Span();
        const Expr& stored = context_->get_expr(name);
        stored.accept(*this);
        visited_.erase(name);
    }

    void Evaluator::visit(const BinaryOp& node) {
        // Evaluate left and right operands in order.
        // Note: result_ is reused for both operands; left_val must be saved
        // before right().
        node.left().accept(*this);
        double left_val = result_;

        node.right().accept(*this);
        double right_val = result_;

        switch (node.op()) {
        case BinaryOpType::Add:
            result_ = left_val + right_val;
            break;
        case BinaryOpType::Sub:
            result_ = left_val - right_val;
            break;
        case BinaryOpType::Mul:
            result_ = left_val * right_val;
            break;
        case BinaryOpType::Div:
            // Division by zero is explicitly checked to avoid undefined
            // behavior.
            if (right_val == 0) {
                throw MathException(errors::math("division by zero",
                                                 node.right().span(), input_));
            }
            result_ = left_val / right_val;
            break;
        case BinaryOpType::Pow:
            // std::pow handles edge cases (e.g., negative bases, fractional
            // exponents) according to IEEE-754 semantics.
            result_ = std::pow(left_val, right_val);
            break;
        }
    }

} // namespace math_solver
