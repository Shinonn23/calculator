#include "evaluator.hpp"
#include "ast/math/binary_expr.hpp"
#include "ast/math/number_expr.hpp"
#include "ast/math/unary_expr.hpp"
#include "ast/math/variable_expr.hpp"
#include "diagnostics/kinds/math_errors.hpp"
#include "diagnostics/kinds/runtime_errors.hpp"
#include "diagnostics/kinds/runtime_errors_extra.hpp"
#include "diagnostics/sink.hpp"
#include <cmath>

namespace math_solver {
    using errors::circular_dependency;
    using errors::math;
    using errors::undefined_variable;

    void Evaluator::visit(const Number& node) {
        // Numbers are terminal nodes; evaluation is trivial.
        result_ = node.value();
    }

    void Evaluator::visit(const Variable& node) {
        // Variable resolution requires a valid context.
        if (!context_) {
            auto d = undefined_variable(node.name(), node.span(), input_);
            if (sink_)
                sink_->push(d);
            return;
        }
        if (!context_->has(node.name())) {
            auto d = undefined_variable(node.name(), node.span(), input_);
            if (sink_)
                sink_->push(d);
            return;
        }

        const std::string& name = node.name();

        // Detect and prevent cycles in variable definitions.
        // This is critical to avoid infinite recursion in cases like `a = a +
        // 1`.
        if (auto it = visited_.find(name); it != visited_.end()) {
            auto d = circular_dependency(name, it->second, input_);
            if (sink_)
                sink_->push(d);
            return;
        }

        // Recursively evaluate the expression bound to this variable.
        // Note: visited_ is used as a dynamic set for the current evaluation
        // stack.
        visited_[name]     = Span();
        const Expr& stored = context_->get_expr(name);
        stored.accept(*this);
        visited_.erase(name);
    }

    void Evaluator::visit(const UnaryOp& node) {
        node.operand().accept(*this);
        switch (node.op()) {
        case UnaryOpType::Neg:
            result_ = -result_;
            break;
        }
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
                auto d = math("division by zero", node.right().span(), input_);
                if (sink_)
                    sink_->push(d);
                break;
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
