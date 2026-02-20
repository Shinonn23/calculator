#ifndef MATH_SOLVER_CONTEXT_HPP
#define MATH_SOLVER_CONTEXT_HPP

#include "ast/math/expr.hpp"
#include <string>
#include <unordered_map>
#include <vector>

namespace math_solver {

    class Context {
        private:
        std::unordered_map<std::string, ExprPtr> variables_;

        public:
        Context() = default;

        void                     set(const std::string& name, const Expr& expr);
        void                     set(const std::string& name, ExprPtr expr);
        void                     set(const std::string& name, double value);

        const Expr&              get_expr(const std::string& name) const;
        bool                     has(const std::string& name) const;
        bool                     unset(const std::string& name);
        void                     clear();

        std::vector<std::string> all_names() const;
        const std::unordered_map<std::string, ExprPtr>& all() const;
        std::unordered_map<std::string, std::string>    all_as_strings() const;

        size_t                                          size() const;
        bool                                            empty() const;
    };

} // namespace math_solver

#endif // MATH_SOLVER_CONTEXT_HPP