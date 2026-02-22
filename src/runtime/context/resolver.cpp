#include "resolver.hpp"
#include "ast/math/binary_expr.hpp"
#include "ast/math/number_expr.hpp"
#include "ast/math/variable_expr.hpp"
#include "core/error.hpp"
#include <cmath>

namespace math_solver {

    double Resolver::evaluate(const Expr& expr, const Context& ctx) {
        // Entry point for expression evaluation.
        // Each evaluation starts with a fresh visited set to ensure
        // variable cycles are detected per top-level call.
        std::unordered_set<std::string> visited;
        return resolve_recursive(expr, ctx, visited);
    }

    double Resolver::evaluate_variable(const std::string& name,
                                       const Context&     ctx) {
        // Evaluates a named variable in the given context.
        // The variable is marked as visited to prevent immediate
        // self-reference.
        std::unordered_set<std::string> visited;
        visited.insert(name);
        return resolve_recursive(ctx.get_expr(name), ctx, visited);
    }

    bool Resolver::try_evaluate(const std::string& name, const Context& ctx,
                                double& out) {
        // Provides a fallible interface for variable evaluation.
        // Any error (undefined variable, circular dependency, math error)
        // results in a false return; no error details are propagated.
        try {
            out = evaluate_variable(name, ctx);
            return true;
        } catch (...) {
            return false;
        }
    }

    double
    Resolver::resolve_recursive(const Expr& expr, const Context& ctx,
                                std::unordered_set<std::string>& visited) {
        // Recursive evaluation of expressions.
        // Invariant: 'visited' tracks the current dependency chain to detect
        // cycles. Assumes 'ctx' remains valid for the duration of recursion.

        if (auto* num = dynamic_cast<const Number*>(&expr)) {
            // Fast path for literals; no dependencies.
            return num->value();
        }

        if (auto* var = dynamic_cast<const Variable*>(&expr)) {
            const std::string& name = var->name();
            // Undefined variables are rejected eagerly.
            if (!ctx.has(name)) {
                throw MathException(
                    errors::undefined_variable(name, var->span()));
            }
            // Detects cycles in variable dependencies.
            if (visited.count(name)) {
                throw MathException(
                    errors::circular_dependency(name, var->span()));
            }
            visited.insert(name);
            double val = resolve_recursive(ctx.get_expr(name), ctx, visited);
            visited.erase(name);
            return val;
        }

        if (auto* bin = dynamic_cast<const BinaryOp*>(&expr)) {
            // Both operands are always evaluated (no short-circuiting).
            // This is required for correct cycle detection and error
            // propagation.
            double left_val  = resolve_recursive(bin->left(), ctx, visited);
            double right_val = resolve_recursive(bin->right(), ctx, visited);

            switch (bin->op()) {
            case BinaryOpType::Add:
                return left_val + right_val;
            case BinaryOpType::Sub:
                return left_val - right_val;
            case BinaryOpType::Mul:
                return left_val * right_val;
            case BinaryOpType::Div:
                // Division by zero is explicitly checked to avoid UB.
                if (right_val == 0)
                    throw MathException(
                        errors::math("division by zero", bin->right().span()));
                return left_val / right_val;
            case BinaryOpType::Pow:
                // std::pow may return NaN or inf for some inputs;
                // caller is responsible for handling such cases if needed.
                return std::pow(left_val, right_val);
            }
        }
        // Defensive: all expression types must be handled above.
        throw std::runtime_error("unknown expression type in resolver");
    }

} // namespace math_solver