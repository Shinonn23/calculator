#ifndef MATH_SOLVER_RESOLVER_HPP
#define MATH_SOLVER_RESOLVER_HPP

#include "ast/math/expr.hpp"
#include "context.hpp"
#include <string>
#include <unordered_set>

namespace math_solver {

    class Resolver {
        public:
        // Entry point for evaluating an expression tree using the provided
        // context.
        // - Assumes 'expr' is well-formed and all referenced variables are
        // resolvable in 'ctx'.
        // - Panics (throws) if evaluation encounters an unbound variable or
        // cyclic dependency.
        // - Used by higher-level passes that require concrete numeric results.
        static double evaluate(const Expr& expr, const Context& ctx);

        // Resolves a variable by name in the given context.
        // - Assumes 'name' is present in 'ctx'; otherwise, behavior is
        // undefined.
        // - Used internally by expression evaluation and by passes that need
        // direct variable access.
        static double evaluate_variable(const std::string& name,
                                        const Context&     ctx);

        // Attempts to evaluate a variable to a numeric value.
        // - Returns true if the variable is fully resolved to a constant.
        // - Returns false if the variable is undefined or depends on unresolved
        // symbols.
        // - Used by constant folding and dead code elimination passes.
        static bool
        try_evaluate(const std::string& name, const Context& ctx, double& out);

        private:
        // Recursive evaluation helper.
        // - Tracks visited variables to detect and prevent cycles (e.g., x = y,
        // y = x).
        // - 'visited' must be empty on entry; mutated in-place.
        // - Assumes 'expr' is acyclic and all variable references are valid in
        // 'ctx'.
        // - Any violation results in immediate termination (panic/throw).
        static double
        resolve_recursive(const Expr&                      expr,
                          const Context&                   ctx,
                          std::unordered_set<std::string>& visited);
    };

} // namespace math_solver

#endif // MATH_SOLVER_RESOLVER_HPP