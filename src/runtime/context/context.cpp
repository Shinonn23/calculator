#include "context.hpp"
#include "ast/math/expr.hpp"
#include "ast/math/number_expr.hpp"

#include <stdexcept>

namespace math_solver {

    Context::Context(const Context& other) {
        for (const auto& [name, expr] : other.variables_) {
            variables_[name] = expr->clone();
        }
    }

    Context& Context::operator=(const Context& other) {
        if (this != &other) {
            variables_.clear();
            for (const auto& [name, expr] : other.variables_) {
                variables_[name] = expr->clone();
            }
        }
        return *this;
    }

    // Overwrites any existing binding for `name`.
    // Clones the input expression to avoid aliasing issues.
    void Context::set(const std::string& name, const Expr& expr) {
        variables_[name] = expr.clone();
    }

    // Transfers ownership of the provided expression pointer.
    // Used when the caller can relinquish ownership, avoiding an extra clone.
    void Context::set(const std::string& name, ExprPtr expr) {
        variables_[name] = std::move(expr);
    }

    // Fast path for numeric constants; avoids heap allocation for Expr
    // wrappers.
    void Context::set(const std::string& name, double value) {
        variables_[name] = std::make_unique<Number>(value);
    }

    // Returns a reference to the stored expression for `name`.
    // Panics if the variable is not present; callers must ensure existence.
    const Expr& Context::get_expr(const std::string& name) const {
        auto it = variables_.find(name);
        if (it == variables_.end()) {
            throw std::runtime_error("undefined variable: " + name);
        }
        return *it->second;
    }

    // Checks for existence of a variable binding.
    bool Context::has(const std::string& name) const {
        return variables_.find(name) != variables_.end();
    }

    // Removes the binding for `name` if present.
    // Returns true if a binding was removed.
    bool Context::unset(const std::string& name) {
        return variables_.erase(name) > 0;
    }

    // Clears all variable bindings.
    // Used to reset the context between evaluation passes.
    void                     Context::clear() { variables_.clear(); }

    // Returns all variable names in insertion order is not guaranteed.
    std::vector<std::string> Context::all_names() const {
        std::vector<std::string> names;
        names.reserve(variables_.size());
        for (const auto& pair : variables_)
            names.push_back(pair.first);
        return names;
    }

    // Exposes the full variable map for introspection.
    // Callers must not mutate the returned map.
    const std::unordered_map<std::string, ExprPtr>& Context::all() const {
        return variables_;
    }

    // Returns a map of variable names to their string representations.
    // Used for diagnostics and debugging output.
    std::unordered_map<std::string, std::string>
    Context::all_as_strings() const {
        std::unordered_map<std::string, std::string> result;
        for (const auto& [name, expr] : variables_) {
            result[name] = expr->to_string();
        }
        return result;
    }

    // Returns the number of variable bindings.
    size_t Context::size() const { return variables_.size(); }

    // Returns true if there are no variable bindings.
    bool   Context::empty() const { return variables_.empty(); }

} // namespace math_solver