#include "evaluator.hpp"
#include "ast/binary.hpp"
#include "ast/number.hpp"
#include "ast/variable.hpp"
#include "common/error.hpp"
#include <cmath>

namespace math_solver {

    void Evaluator::visit(const Number& node) { result_ = node.value(); }

    void Evaluator::visit(const Variable& node) {
        if (!context_) {
            throw UndefinedVariableError(node.name(), node.span(), input_);
        }
        if (!context_->has(node.name())) {
            throw UndefinedVariableError(node.name(), node.span(), input_);
        }

        const std::string& name = node.name();

        // Cycle detection
        if (visited_.count(name)) {
            throw CircularDependencyError(name, node.span(), input_);
        }

        // Recursively evaluate the stored expression
        visited_.insert(name);
        const Expr& stored = context_->get_expr(name);
        stored.accept(*this);
        visited_.erase(name);
    }

    void Evaluator::visit(const BinaryOp& node) {
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
            if (right_val == 0) {
                throw MathError("division by zero", node.right().span(),
                                input_);
            }
            result_ = left_val / right_val;
            break;
        case BinaryOpType::Pow:
            result_ = std::pow(left_val, right_val);
            break;
        }
    }

} // namespace math_solver
