#include "resolver.hpp"
#include "ast/math/binary_expr.hpp"
#include "ast/math/number_expr.hpp"
#include "ast/math/variable_expr.hpp"
#include "core/error.hpp"
#include <cmath>

namespace math_solver {

    double Resolver::evaluate(const Expr& expr, const Context& ctx) {
        std::unordered_set<std::string> visited;
        return resolve_recursive(expr, ctx, visited);
    }

    double Resolver::evaluate_variable(const std::string& name,
                                       const Context&     ctx) {
        std::unordered_set<std::string> visited;
        visited.insert(name);
        return resolve_recursive(ctx.get_expr(name), ctx, visited);
    }

    bool Resolver::try_evaluate(const std::string& name, const Context& ctx,
                                double& out) {
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
        if (auto* num = dynamic_cast<const Number*>(&expr)) {
            return num->value();
        }

        if (auto* var = dynamic_cast<const Variable*>(&expr)) {
            const std::string& name = var->name();
            if (!ctx.has(name)) {
                throw UndefinedVariableError(name, var->span());
            }
            if (visited.count(name)) {
                throw CircularDependencyError(name, var->span());
            }
            visited.insert(name);
            double val = resolve_recursive(ctx.get_expr(name), ctx, visited);
            visited.erase(name);
            return val;
        }

        if (auto* bin = dynamic_cast<const BinaryOp*>(&expr)) {
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
                if (right_val == 0)
                    throw MathError("division by zero", bin->right().span());
                return left_val / right_val;
            case BinaryOpType::Pow:
                return std::pow(left_val, right_val);
            }
        }
        throw std::runtime_error("unknown expression type in resolver");
    }

} // namespace math_solver