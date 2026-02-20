#ifndef MATH_SOLVER_RESOLVER_HPP
#define MATH_SOLVER_RESOLVER_HPP

#include "ast/math/expr.hpp"
#include "context.hpp"
#include <string>
#include <unordered_set>

namespace math_solver {

    class Resolver {
        public:
        // Evaluates a given expression using bindings from the context
        static double evaluate(const Expr& expr, const Context& ctx);

        // Evaluates a variable by its name
        static double evaluate_variable(const std::string& name,
                                        const Context&     ctx);

        // Try to evaluate a variable, returning true if fully numeric
        static bool   try_evaluate(const std::string& name, const Context& ctx,
                                   double& out);

        private:
        static double
        resolve_recursive(const Expr& expr, const Context& ctx,
                          std::unordered_set<std::string>& visited);
    };

} // namespace math_solver

#endif // MATH_SOLVER_RESOLVER_HPP