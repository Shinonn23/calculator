#pragma once

#include "ast/math/expr.hpp"
#include <string>
#include <unordered_map>
#include <vector>

namespace math_solver {

    // Context provides a mapping from variable names to expressions.
    //
    // Invariants:
    // - Each variable name is unique within the context.
    // - All stored ExprPtr must be valid and non-null.
    //
    // Performance:
    // - Lookups and insertions are O(1) amortized due to unordered_map.
    //
    // Correctness:
    // - set() will overwrite any existing binding for a given name.
    // - unset() returns true if a binding was present and removed.
    //
    // Interactions:
    // - Used by the evaluator and simplifier passes to resolve variable
    // bindings.
    // - Must remain consistent with any external state that depends on variable
    // bindings.
    class Context {
        private:
        std::unordered_map<std::string, ExprPtr> variables_;

        public:
        Context() = default;
        Context(const Context& other);
        Context& operator=(const Context& other);
        Context(Context&&) noexcept               = default;
        Context&    operator=(Context&&) noexcept = default;

        // Overwrites or inserts a variable binding.
        // The Expr is copied or moved as appropriate.
        void        set(const std::string& name, const Expr& expr);
        void        set(const std::string& name, ExprPtr expr);
        void        set(const std::string& name, double value);
        // Stores a list of numeric values as an ArrayExpr.
        void        set(const std::string& name, std::vector<double> values);

        // Returns a reference to the expression bound to `name`.
        // UB if `name` is not present; caller must check with has().
        const Expr& get_expr(const std::string& name) const;

        // Returns true if `name` is bound in this context.
        bool        has(const std::string& name) const;

        // Removes the binding for `name` if present.
        // Returns true if a binding was removed.
        bool        unset(const std::string& name);

        // Removes all variable bindings.
        void        clear();

        // Returns all variable names in insertion order is NOT guaranteed.
        std::vector<std::string>                        all_names() const;

        // Returns a const reference to the internal variable map.
        // Exposed for bulk inspection; mutating the returned map is UB.
        const std::unordered_map<std::string, ExprPtr>& all() const;

        Context clone(const Context& ctx) { return Context(ctx); }

        // Returns a map of variable names to stringified expressions.
        // Used for diagnostics and debugging output.
        std::unordered_map<std::string, std::string> all_as_strings() const;

        // Returns the number of variable bindings.
        size_t                                       size() const;

        // Returns true if there are no variable bindings.
        bool                                         empty() const;
    };

} // namespace math_solver
