#include "resolver.hpp"
#include "ast/math/binary_expr.hpp"
#include "ast/math/number_expr.hpp"
#include "ast/math/variable_expr.hpp"
#include "diagnostics/kinds/math_errors.hpp"
#include "diagnostics/kinds/runtime_errors.hpp"
#include "diagnostics/kinds/runtime_errors_extra.hpp"
#include <cmath>

namespace math_solver {

    Result<double> Resolver::evaluate(const Expr& expr, const Context& ctx) {
        // Entry point for expression evaluation.
        // Each evaluation starts with a fresh visited set to ensure
        // variable cycles are detected per top-level call.
        std::unordered_set<std::string> visited;
        return resolve_recursive(expr, ctx, visited);
    }

    Result<double> Resolver::evaluate_variable(const std::string& name,
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
        auto res = evaluate_variable(name, ctx);
        if (res) {
            out = *res;
            return true;
        }
        return false;
    }

    Result<double>
    Resolver::resolve_recursive(const Expr& expr, const Context& ctx,
                                std::unordered_set<std::string>& visited) {
        // Recursive evaluation of expressions.
        // Invariant: 'visited' tracks the current dependency chain to detect
        // cycles. Assumes 'ctx' remains valid for the duration of recursion.

        if (auto* num = dynamic_cast<const Number*>(&expr)) {
            // Fast path for literals; no dependencies.
            return Result<double>::ok(num->value());
        }

        if (auto* var = dynamic_cast<const Variable*>(&expr)) {
            const std::string& name = var->name();
            // Undefined variables are rejected eagerly.
            if (!ctx.has(name)) {
                return Result<double>::err(
                    errors::undefined_variable(name, var->span()));
            }
            // Detects cycles in variable dependencies.
            if (visited.count(name)) {
                return Result<double>::err(
                    errors::circular_dependency(name, var->span()));
            }
            visited.insert(name);
            auto val = resolve_recursive(ctx.get_expr(name), ctx, visited);
            visited.erase(name);
            return val;
        }

        if (auto* bin = dynamic_cast<const BinaryOp*>(&expr)) {
            // Both operands are always evaluated (no short-circuiting).
            // This is required for correct cycle detection and error
            // propagation.
            auto left_r = resolve_recursive(bin->left(), ctx, visited);
            if (!left_r.ok())
                return left_r;
            auto right_r = resolve_recursive(bin->right(), ctx, visited);
            if (!right_r.ok())
                return right_r;

            double left_val  = *left_r;
            double right_val = *right_r;

            switch (bin->op()) {
            case BinaryOpType::Add:
                return Result<double>::ok(left_val + right_val);
            case BinaryOpType::Sub:
                return Result<double>::ok(left_val - right_val);
            case BinaryOpType::Mul:
                return Result<double>::ok(left_val * right_val);
            case BinaryOpType::Div:
                // Division by zero is explicitly checked to avoid UB.
                if (right_val == 0)
                    return Result<double>::err(
                        errors::math("division by zero", bin->right().span()));
                return Result<double>::ok(left_val / right_val);
            case BinaryOpType::Pow:
                // std::pow may return NaN or inf for some inputs;
                // caller is responsible for handling such cases if needed.
                return Result<double>::ok(std::pow(left_val, right_val));
            }
        }
        // Defensive: all expression types must be handled above.
        return Result<double>::err(Diagnostic::make(
            "unknown expression type in resolver", "E0000", expr.span()));
    }

} // namespace math_solver