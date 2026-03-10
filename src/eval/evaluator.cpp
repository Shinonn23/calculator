#include "evaluator.hpp"
#include "ast/math/array_expr.hpp"
#include "ast/math/binary_expr.hpp"
#include "ast/math/call_expr.hpp"
#include "ast/math/number_expr.hpp"
#include "ast/math/unary_expr.hpp"
#include "ast/math/variable_expr.hpp"
#include "diagnostics/kinds/math_errors.hpp"
#include "diagnostics/kinds/runtime_errors.hpp"
#include "diagnostics/kinds/runtime_errors_extra.hpp"
#include "diagnostics/sink.hpp"
#include <cmath>
#include <set>
#include <string>
#include <vector>

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
            auto d = undefined_variable(node.name(), node.span(), input_,
                                        source_file_, source_line_);
            if (sink_)
                sink_->push(d);
            return;
        }
        if (!context_->has(node.name())) {
            auto d = undefined_variable(node.name(), node.span(), input_,
                                        source_file_, source_line_);
            if (sink_)
                sink_->push(d);
            return;
        }

        const std::string& name = node.name();

        // Detect and prevent cycles in variable definitions.
        // This is critical to avoid infinite recursion in cases like `a = a +
        // 1`.
        if (auto it = visited_.find(name); it != visited_.end()) {
            auto d = circular_dependency(name, it->second, input_, source_file_,
                                         source_line_);
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
                auto d = math("division by zero", node.right().span(), input_,
                              source_file_, source_line_);
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

    void Evaluator::visit(const ArrayExpr& node) {
        if (broadcast_index_ >= 0 &&
            static_cast<size_t>(broadcast_index_) < node.size()) {
            node.elements()[static_cast<size_t>(broadcast_index_)]->accept(
                *this);
            return;
        }

        if (node.size() == 0) {
            auto d = errors::math("empty array", node.span(), input_,
                                  source_file_, source_line_);
            if (sink_)
                sink_->push(d);
            result_ = 0.0;
            return;
        }

        // Arrays cannot be used directly in scalar expression evaluation.
        // The caller should use evaluate_broadcast() when array variables are
        // expected.
        auto d = errors::math("array value cannot be used in scalar context",
                              node.span(), input_, source_file_, source_line_);
        if (sink_)
            sink_->push(d);
        result_ = 0.0;
    }

    void Evaluator::visit(const FunctionCall& node) {
        node.arg().accept(*this);
        double x = result_;
        switch (node.kind()) {
        case FuncKind::Sin:
            result_ = std::sin(x);
            break;
        case FuncKind::Cos:
            result_ = std::cos(x);
            break;
        case FuncKind::Tan:
            result_ = std::tan(x);
            break;
        case FuncKind::Asin:
            if (x < -1.0 || x > 1.0) {
                auto d = errors::func_domain(
                    "domain error: asin argument must be in [-1, 1]",
                    node.span(), input_);
                if (sink_)
                    sink_->push(d);
                result_ = 0.0;
                return;
            }
            result_ = std::asin(x);
            break;
        case FuncKind::Acos:
            if (x < -1.0 || x > 1.0) {
                auto d = errors::func_domain(
                    "domain error: acos argument must be in [-1, 1]",
                    node.span(), input_);
                if (sink_)
                    sink_->push(d);
                result_ = 0.0;
                return;
            }
            result_ = std::acos(x);
            break;
        case FuncKind::Atan:
            result_ = std::atan(x);
            break;
        case FuncKind::Sinh:
            result_ = std::sinh(x);
            break;
        case FuncKind::Cosh:
            result_ = std::cosh(x);
            break;
        case FuncKind::Tanh:
            result_ = std::tanh(x);
            break;
        case FuncKind::Exp:
            result_ = std::exp(x);
            break;
        case FuncKind::Sqrt:
            if (x < 0.0) {
                auto d =
                    errors::func_domain("domain error: sqrt of negative number",
                                        node.span(), input_);
                if (sink_)
                    sink_->push(d);
                result_ = 0.0;
                return;
            }
            result_ = std::sqrt(x);
            break;
        case FuncKind::Ln:
            if (x <= 0.0) {
                auto d = errors::func_domain(
                    "domain error: ln argument must be positive", node.span(),
                    input_);
                if (sink_)
                    sink_->push(d);
                result_ = 0.0;
                return;
            }
            result_ = std::log(x);
            break;
        case FuncKind::Log:
            if (x <= 0.0) {
                auto d = errors::func_domain(
                    "domain error: log argument must be positive", node.span(),
                    input_);
                if (sink_)
                    sink_->push(d);
                result_ = 0.0;
                return;
            }
            result_ = std::log10(x);
            break;
        case FuncKind::Abs:
            result_ = std::abs(x);
            break;
        case FuncKind::Floor:
            result_ = std::floor(x);
            break;
        case FuncKind::Ceil:
            result_ = std::ceil(x);
            break;
        case FuncKind::Round:
            result_ = std::round(x);
            break;
        }
    }

    // Broadcast evaluation: evaluates `expr` for each element of any
    // array-bound variable found in the expression. All array variables must
    // have the same length; otherwise an error is pushed to sink.
    //
    // Algorithm:
    //   1. Walk expr to collect all VariableExpr names.
    //   2. For each name present in ctx, check whether it is an ArrayExpr.
    //   3. Require consistent array size across all array-bound variables.
    //   4. Iterate: bind each array variable to its i-th element in a temporary
    //      context, evaluate, and record the result.
    std::vector<double> Evaluator::evaluate_broadcast(const Expr&    expr,
                                                      const Context& ctx) {
        // Step 1: collect all variable names referenced by the expression.
        // Use a tiny visitor that only tracks Variable nodes.
        class VarNameCollector : public ExprVisitor {
            public:
            std::set<std::string> names;
            void                  visit(const Number&) override {}
            void                  visit(const BinaryOp& n) override {
                n.left().accept(*this);
                n.right().accept(*this);
            }
            void visit(const UnaryOp& n) override { n.operand().accept(*this); }
            void visit(const Variable& n) override { names.insert(n.name()); }
            void visit(const ArrayExpr& n) override {
                for (const auto& e : n.elements())
                    e->accept(*this);
            }
            void visit(const FunctionCall& n) override {
                n.arg().accept(*this);
            }
        };

        VarNameCollector col;
        expr.accept(col);

        // Step 2a: find array-bound variables and determine array size.
        std::vector<std::string> arr_vars;
        size_t                   arr_size = 0;

        for (const auto& name : col.names) {
            if (!ctx.has(name))
                continue;
            const auto* arr =
                dynamic_cast<const ArrayExpr*>(&ctx.get_expr(name));
            if (!arr)
                continue;
            if (arr_vars.empty()) {
                arr_size = arr->size();
            } else if (arr->size() != arr_size) {
                auto d = errors::math(
                    "array size mismatch: '" + arr_vars[0] + "' has " +
                        std::to_string(arr_size) + " elements but '" + name +
                        "' has " + std::to_string(arr->size()),
                    Span{}, input_, source_file_, source_line_);
                if (sink_)
                    sink_->push(d);
                return {};
            }
            arr_vars.push_back(name);
        }

        // Step 2b: find inline ArrayExpr literals and check size consistency.
        class ArrayLiteralFinder : public ExprVisitor {
            public:
            std::vector<const ArrayExpr*> arrays;
            void                          visit(const Number&) override {}
            void                          visit(const BinaryOp& n) override {
                n.left().accept(*this);
                n.right().accept(*this);
            }
            void visit(const UnaryOp& n) override { n.operand().accept(*this); }
            void visit(const Variable&) override {}
            void visit(const ArrayExpr& n) override { arrays.push_back(&n); }
            void visit(const FunctionCall& n) override {
                n.arg().accept(*this);
            }
        };

        ArrayLiteralFinder finder;
        expr.accept(finder);

        for (const auto* arr : finder.arrays) {
            if (arr_size == 0) {
                arr_size = arr->size();
            } else if (arr->size() != arr_size) {
                auto d = errors::math(
                    "array size mismatch: array literal has " +
                        std::to_string(arr->size()) +
                        " elements but expected " + std::to_string(arr_size),
                    arr->span(), input_, source_file_, source_line_);
                if (sink_)
                    sink_->push(d);
                return {};
            }
        }

        bool                has_inline_arrays = !finder.arrays.empty();

        // Step 3: iterate and evaluate.
        std::vector<double> results;
        results.reserve(arr_size);

        for (size_t i = 0; i < arr_size; ++i) {
            // Build a temporary context: copy parent, override array vars with
            // their i-th element.
            Context temp = ctx;
            for (const auto& var : arr_vars) {
                const auto& arr =
                    dynamic_cast<const ArrayExpr&>(ctx.get_expr(var));
                temp.set(var, *arr.elements()[i]);
            }
            Evaluator elem_eval(&temp, input_, sink_);
            elem_eval.set_source(source_file_, source_line_);
            if (has_inline_arrays)
                elem_eval.set_broadcast_index(static_cast<int>(i));
            results.push_back(elem_eval.evaluate(expr));
        }

        return results;
    }

} // namespace math_solver
