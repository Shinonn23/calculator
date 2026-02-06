#ifndef CONSTANT_FOLDER_H
#define CONSTANT_FOLDER_H

#include "ast/binary.h"
#include "ast/expr.h"
#include "ast/function_call.h"
#include "ast/index_access.h"
#include "ast/number.h"
#include "ast/number_array.h"
#include "ast/variable.h"
#include <cmath>
#include <memory>
#include <string>
#include <vector>

namespace math_solver {

    // Constant folding: walks an AST and evaluates all sub-expressions
    // that consist entirely of numeric values, replacing them with Number
    // nodes. e.g., (2*7 + 1)^2 - 4 + 9 → 230
    class ConstantFolder : public ExprVisitor {
        private:
        ExprPtr       result_;

        // Evaluate a single-argument built-in function on a constant
        static double eval_func(const std::string& name, double x) {
            if (name == "sqrt")
                return std::sqrt(x);
            if (name == "abs")
                return std::abs(x);
            if (name == "sin")
                return std::sin(x);
            if (name == "cos")
                return std::cos(x);
            if (name == "tan")
                return std::tan(x);
            if (name == "log")
                return std::log10(x);
            if (name == "ln")
                return std::log(x);
            if (name == "exp")
                return std::exp(x);
            if (name == "floor")
                return std::floor(x);
            if (name == "ceil")
                return std::ceil(x);
            return std::numeric_limits<double>::quiet_NaN();
        }

        public:
        // Fold constants in an expression, returning a new (potentially
        // simpler) tree.
        ExprPtr fold(const Expr& expr) {
            expr.accept(*this);
            return std::move(result_);
        }

        void visit(const Number& node) override { result_ = node.clone(); }

        void visit(const Variable& node) override { result_ = node.clone(); }

        void visit(const NumberArray& node) override { result_ = node.clone(); }

        void visit(const IndexAccess& node) override {
            // Fold the target
            node.target().accept(*this);
            ExprPtr folded_target = std::move(result_);

            // If target folded to a NumberArray, resolve the index
            auto*   arr = dynamic_cast<const NumberArray*>(folded_target.get());
            if (arr && node.index() < arr->size()) {
                result_ = std::make_unique<Number>(arr->at(node.index()),
                                                   node.span());
                return;
            }

            // Can't fold — rebuild
            auto rebuilt = std::make_unique<IndexAccess>(
                std::move(folded_target), node.index());
            rebuilt->set_span(node.span());
            result_ = std::move(rebuilt);
        }

        void visit(const FunctionCall& node) override {
            // Fold arguments first
            std::vector<ExprPtr> folded_args;
            for (size_t i = 0; i < node.arg_count(); ++i) {
                node.arg(i).accept(*this);
                folded_args.push_back(std::move(result_));
            }

            // Single-argument function: try scalar fold or array map
            if (folded_args.size() == 1) {
                auto* num = dynamic_cast<const Number*>(folded_args[0].get());
                if (num) {
                    double val = eval_func(node.name(), num->value());
                    if (!std::isnan(val) && std::isfinite(val)) {
                        result_ = std::make_unique<Number>(val, node.span());
                        return;
                    }
                }

                // Array argument → map function element-wise
                auto* arr =
                    dynamic_cast<const NumberArray*>(folded_args[0].get());
                if (arr) {
                    std::vector<double> out;
                    out.reserve(arr->size());
                    bool all_ok = true;
                    for (size_t i = 0; i < arr->size(); ++i) {
                        double v = eval_func(node.name(), arr->at(i));
                        if (std::isnan(v) || !std::isfinite(v)) {
                            all_ok = false;
                            break;
                        }
                        out.push_back(v);
                    }
                    if (all_ok) {
                        result_ =
                            std::make_unique<NumberArray>(out, node.span());
                        return;
                    }
                }
            }

            // Can't fully fold — rebuild with folded args
            auto folded = std::make_unique<FunctionCall>(
                node.name(), std::move(folded_args));
            folded->set_span(node.span());
            result_ = std::move(folded);
        }

        // Apply a binary operation to two scalars
        static double apply_op(BinaryOpType op, double lv, double rv) {
            switch (op) {
            case BinaryOpType::Add:
                return lv + rv;
            case BinaryOpType::Sub:
                return lv - rv;
            case BinaryOpType::Mul:
                return lv * rv;
            case BinaryOpType::Div:
                return lv / rv;
            case BinaryOpType::Pow:
                return std::pow(lv, rv);
            }
            return std::numeric_limits<double>::quiet_NaN();
        }

        void visit(const BinaryOp& node) override {
            node.left().accept(*this);
            ExprPtr left = std::move(result_);

            node.right().accept(*this);
            ExprPtr right     = std::move(result_);

            auto*   left_num  = dynamic_cast<const Number*>(left.get());
            auto*   right_num = dynamic_cast<const Number*>(right.get());
            auto*   left_arr  = dynamic_cast<const NumberArray*>(left.get());
            auto*   right_arr = dynamic_cast<const NumberArray*>(right.get());

            // scalar ⊕ scalar
            if (left_num && right_num) {
                double lv = left_num->value();
                double rv = right_num->value();

                // Guard division by zero
                if (node.op() == BinaryOpType::Div && std::abs(rv) < 1e-15) {
                    auto rebuilt = std::make_unique<BinaryOp>(
                        std::move(left), std::move(right), node.op());
                    rebuilt->set_span(node.span());
                    result_ = std::move(rebuilt);
                    return;
                }

                double val = apply_op(node.op(), lv, rv);
                if (std::isfinite(val)) {
                    result_ = std::make_unique<Number>(val, node.span());
                    return;
                }
            }

            // scalar ⊕ array  →  broadcast
            if (left_num && right_arr) {
                std::vector<double> out;
                out.reserve(right_arr->size());
                for (size_t i = 0; i < right_arr->size(); ++i) {
                    double v = apply_op(node.op(), left_num->value(),
                                        right_arr->at(i));
                    if (!std::isfinite(v))
                        goto cant_fold;
                    out.push_back(v);
                }
                result_ = std::make_unique<NumberArray>(out, node.span());
                return;
            }

            // array ⊕ scalar  →  broadcast
            if (left_arr && right_num) {
                std::vector<double> out;
                out.reserve(left_arr->size());
                for (size_t i = 0; i < left_arr->size(); ++i) {
                    double v = apply_op(node.op(), left_arr->at(i),
                                        right_num->value());
                    if (!std::isfinite(v))
                        goto cant_fold;
                    out.push_back(v);
                }
                result_ = std::make_unique<NumberArray>(out, node.span());
                return;
            }

            // array ⊕ array  →  element-wise (same size)
            if (left_arr && right_arr &&
                left_arr->size() == right_arr->size()) {
                std::vector<double> out;
                out.reserve(left_arr->size());
                for (size_t i = 0; i < left_arr->size(); ++i) {
                    double v =
                        apply_op(node.op(), left_arr->at(i), right_arr->at(i));
                    if (!std::isfinite(v))
                        goto cant_fold;
                    out.push_back(v);
                }
                result_ = std::make_unique<NumberArray>(out, node.span());
                return;
            }

        cant_fold:
            // Can't fully fold — rebuild with folded children
            auto rebuilt = std::make_unique<BinaryOp>(
                std::move(left), std::move(right), node.op());
            rebuilt->set_span(node.span());
            result_ = std::move(rebuilt);
        }
    };

    // Convenience function
    inline ExprPtr fold_constants(const Expr& expr) {
        ConstantFolder folder;
        return folder.fold(expr);
    }

} // namespace math_solver

#endif
