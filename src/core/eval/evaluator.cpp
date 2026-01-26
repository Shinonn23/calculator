#include "evaluator.h"
#include "../ast/binary.h"
#include "../ast/number.h"
#include <cmath>
#include <stdexcept>

namespace math_solver {

    void Evaluator::visit(const Number& node) { result_ = node.value(); }

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
                throw std::runtime_error("Division by zero");
            }
            result_ = left_val / right_val;
            break;
        case BinaryOpType::Pow:
            result_ = std::pow(left_val, right_val);
            break;
        }
    }

} // namespace math_solver
