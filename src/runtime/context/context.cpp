#include "context.hpp"
#include "ast/math/expr.hpp"
#include "ast/math/number_expr.hpp"

#include <stdexcept>

namespace math_solver {

    void Context::set(const std::string& name, const Expr& expr) {
        variables_[name] = expr.clone();
    }

    void Context::set(const std::string& name, ExprPtr expr) {
        variables_[name] = std::move(expr);
    }

    void Context::set(const std::string& name, double value) {
        variables_[name] = std::make_unique<Number>(value);
    }

    const Expr& Context::get_expr(const std::string& name) const {
        auto it = variables_.find(name);
        if (it == variables_.end()) {
            throw std::runtime_error("undefined variable: " + name);
        }
        return *it->second;
    }

    bool Context::has(const std::string& name) const {
        return variables_.find(name) != variables_.end();
    }

    bool Context::unset(const std::string& name) {
        return variables_.erase(name) > 0;
    }

    void                     Context::clear() { variables_.clear(); }

    std::vector<std::string> Context::all_names() const {
        std::vector<std::string> names;
        names.reserve(variables_.size());
        for (const auto& pair : variables_)
            names.push_back(pair.first);
        return names;
    }

    const std::unordered_map<std::string, ExprPtr>& Context::all() const {
        return variables_;
    }

    std::unordered_map<std::string, std::string>
    Context::all_as_strings() const {
        std::unordered_map<std::string, std::string> result;
        for (const auto& [name, expr] : variables_) {
            result[name] = expr->to_string();
        }
        return result;
    }

    size_t Context::size() const { return variables_.size(); }
    bool   Context::empty() const { return variables_.empty(); }

} // namespace math_solver