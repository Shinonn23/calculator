#include "evaluator.h"
#include "ast/binary.h"
#include "ast/function_call.h"
#include "ast/index_access.h"
#include "ast/number.h"
#include "ast/number_array.h"
#include "ast/variable.h"
#include "common/error.h"
#include <cmath>
#include <stdexcept>

namespace math_solver {

    // ================================================================
    // Static helpers: element-wise binary and function application
    // ================================================================

    static double apply_scalar_op(double lv, double rv, BinaryOpType op,
                                  const Span& span, const std::string& input) {
        switch (op) {
        case BinaryOpType::Add:
            return lv + rv;
        case BinaryOpType::Sub:
            return lv - rv;
        case BinaryOpType::Mul:
            return lv * rv;
        case BinaryOpType::Div:
            if (rv == 0) {
                throw MathError("division by zero", span, input);
            }
            return lv / rv;
        case BinaryOpType::Pow:
            return std::pow(lv, rv);
        }
        return 0.0; // unreachable
    }

    static double apply_scalar_func(const std::string& name, double x,
                                    const Span&        span,
                                    const std::string& input) {
        if (name == "sqrt") {
            if (x < 0) {
                throw MathError("sqrt of negative number (" +
                                    std::to_string(x) + ")",
                                span, input);
            }
            return std::sqrt(x);
        } else if (name == "abs") {
            return std::abs(x);
        } else if (name == "sin") {
            return std::sin(x);
        } else if (name == "cos") {
            return std::cos(x);
        } else if (name == "tan") {
            return std::tan(x);
        } else if (name == "log") {
            if (x <= 0) {
                throw MathError("log of non-positive number", span, input);
            }
            return std::log10(x);
        } else if (name == "ln") {
            if (x <= 0) {
                throw MathError("ln of non-positive number", span, input);
            }
            return std::log(x);
        } else if (name == "exp") {
            return std::exp(x);
        } else if (name == "floor") {
            return std::floor(x);
        } else if (name == "ceil") {
            return std::ceil(x);
        }
        throw MathError("unknown function '" + name + "'", span, input);
    }

    // ── Binary broadcasting ─────────────────────────────────────
    // scalar op scalar → scalar
    // scalar op vector → broadcast
    // vector op scalar → broadcast
    // vector op vector → element-wise (sizes must match)
    Value Evaluator::apply_binary(const Value& left, const Value& right,
                                  BinaryOpType op, const Span& span,
                                  const std::string& input) {
        if (left.is_scalar() && right.is_scalar()) {
            double r = apply_scalar_op(left.as_scalar(), right.as_scalar(), op,
                                       span, input);
            return Value::scalar(r);
        }

        if (left.is_scalar() && right.is_vector()) {
            double              lv = left.as_scalar();
            const auto&         rv = right.as_vector();
            std::vector<double> out;
            out.reserve(rv.size());
            for (double v : rv) {
                out.push_back(apply_scalar_op(lv, v, op, span, input));
            }
            return Value::vector(std::move(out));
        }

        if (left.is_vector() && right.is_scalar()) {
            const auto&         lv = left.as_vector();
            double              rv = right.as_scalar();
            std::vector<double> out;
            out.reserve(lv.size());
            for (double v : lv) {
                out.push_back(apply_scalar_op(v, rv, op, span, input));
            }
            return Value::vector(std::move(out));
        }

        // vector op vector
        const auto& lv = left.as_vector();
        const auto& rv = right.as_vector();
        if (lv.size() != rv.size()) {
            throw MathError(
                "vector size mismatch: " + std::to_string(lv.size()) + " vs " +
                    std::to_string(rv.size()),
                span, input);
        }
        std::vector<double> out;
        out.reserve(lv.size());
        for (size_t i = 0; i < lv.size(); ++i) {
            out.push_back(apply_scalar_op(lv[i], rv[i], op, span, input));
        }
        return Value::vector(std::move(out));
    }

    // ── Function broadcasting ───────────────────────────────────
    Value Evaluator::apply_func(const std::string& name, const Value& arg,
                                const Span& span, const std::string& input) {
        if (arg.is_scalar()) {
            double r = apply_scalar_func(name, arg.as_scalar(), span, input);
            return Value::scalar(r);
        }

        // Map function over vector elements
        const auto&         vec = arg.as_vector();
        std::vector<double> out;
        out.reserve(vec.size());
        for (size_t i = 0; i < vec.size(); ++i) {
            try {
                out.push_back(apply_scalar_func(name, vec[i], span, input));
            } catch (const MathError&) {
                throw MathError(
                    name + ": domain error at element [" + std::to_string(i) +
                        "] (value = " + std::to_string(vec[i]) + ")",
                    span, input);
            }
        }
        return Value::vector(std::move(out));
    }

    // ================================================================
    // Visitor implementations
    // ================================================================

    void Evaluator::visit(const Number& node) {
        result_ = Value::scalar(node.value());
    }

    void Evaluator::visit(const Variable& node) {
        if (!context_) {
            throw UndefinedVariableError(node.name(), node.span(), input_);
        }

        const Expr* stored = context_->get_expr(node.name());
        if (!stored) {
            throw UndefinedVariableError(node.name(), node.span(), input_);
        }

        // Guard against circular evaluation (RAII — safe on throw)
        if (evaluating_.count(node.name())) {
            throw MathError("circular variable reference involving '" +
                                node.name() + "'",
                            node.span(), input_);
        }
        EvalGuard guard(evaluating_, node.name());
        stored->accept(*this);
    }

    void Evaluator::visit(const BinaryOp& node) {
        node.left().accept(*this);
        Value left_val = result_;

        node.right().accept(*this);
        Value right_val = result_;

        result_ =
            apply_binary(left_val, right_val, node.op(), node.span(), input_);
    }

    void Evaluator::visit(const FunctionCall& node) {
        const std::string& name = node.name();

        if (node.arg_count() != 1) {
            throw MathError("function '" + name + "' expects 1 argument, got " +
                                std::to_string(node.arg_count()),
                            node.span(), input_);
        }

        node.arg(0).accept(*this);
        Value arg = result_;

        result_   = apply_func(name, arg, node.span(), input_);
    }

    void Evaluator::visit(const NumberArray& node) {
        // Return the array as a vector Value.
        // Single-element arrays become scalars for compatibility.
        if (node.size() == 1) {
            result_ = Value::scalar(node.at(0));
        } else {
            result_ = Value::vector(node.values());
        }
    }

    void Evaluator::visit(const IndexAccess& node) {
        // The target should be a NumberArray (or a variable that resolves to
        // one). First, try to get the target as a variable.
        auto* var = dynamic_cast<const Variable*>(&node.target());
        if (var && context_) {
            const Expr* stored = context_->get_expr(var->name());
            if (stored) {
                auto* arr = dynamic_cast<const NumberArray*>(stored);
                if (arr) {
                    if (node.index() < arr->size()) {
                        result_ = Value::scalar(arr->at(node.index()));
                        return;
                    } else {
                        throw MathError(
                            "index " + std::to_string(node.index()) +
                                " out of range (array '" + var->name() +
                                "' has " + std::to_string(arr->size()) +
                                " elements)",
                            node.span(), input_);
                    }
                }
            }
        }

        // Try evaluating the target — it might produce a vector Value
        node.target().accept(*this);
        if (result_.is_vector()) {
            const auto& vec = result_.as_vector();
            if (node.index() < vec.size()) {
                result_ = Value::scalar(vec[node.index()]);
                return;
            } else {
                throw MathError("index " + std::to_string(node.index()) +
                                    " out of range (array has " +
                                    std::to_string(vec.size()) + " elements)",
                                node.span(), input_);
            }
        }

        throw MathError("cannot index into non-array expression", node.span(),
                        input_);
    }

} // namespace math_solver
