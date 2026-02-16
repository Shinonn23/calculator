#ifndef CONTEXT_H
#define CONTEXT_H

#include "ast/binary.hpp"
#include "ast/expr.hpp"
#include "ast/number.hpp"
#include "ast/variable.hpp"
#include "common/error.hpp"
#include <cmath>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace math_solver {

    // Stores variable bindings as symbolic expressions.
    // Variables may reference other variables; evaluation is lazy.
    class Context {
        private:
        std::unordered_map<std::string, ExprPtr> variables_;

        // Recursively evaluate an expression, resolving variable references.
        // Throws CircularDependencyError if a cycle is detected.
        double resolve_(const Expr& expr,
                        std::unordered_set<std::string>& visited) const {
            // Number literal
            if (auto* num = dynamic_cast<const Number*>(&expr)) {
                return num->value();
            }

            // Variable reference — look up and recurse
            if (auto* var = dynamic_cast<const Variable*>(&expr)) {
                const std::string& name = var->name();
                auto               it   = variables_.find(name);
                if (it == variables_.end()) {
                    throw UndefinedVariableError(name, var->span());
                }
                if (visited.count(name)) {
                    throw CircularDependencyError(name, var->span());
                }
                visited.insert(name);
                double val = resolve_(*it->second, visited);
                visited.erase(name);
                return val;
            }

            // Binary operation — evaluate children
            if (auto* bin = dynamic_cast<const BinaryOp*>(&expr)) {
                double left_val  = resolve_(bin->left(), visited);
                double right_val = resolve_(bin->right(), visited);

                switch (bin->op()) {
                case BinaryOpType::Add:
                    return left_val + right_val;
                case BinaryOpType::Sub:
                    return left_val - right_val;
                case BinaryOpType::Mul:
                    return left_val * right_val;
                case BinaryOpType::Div:
                    if (right_val == 0) {
                        throw MathError("division by zero",
                                        bin->right().span());
                    }
                    return left_val / right_val;
                case BinaryOpType::Pow:
                    return std::pow(left_val, right_val);
                }
            }

            throw std::runtime_error("unknown expression type in context");
        }

        public:
        Context() = default;

        // Set a variable to a symbolic expression (clones the expr)
        void set(const std::string& name, const Expr& expr) {
            variables_[name] = expr.clone();
        }

        // Set a variable to a symbolic expression (takes ownership)
        void set(const std::string& name, ExprPtr expr) {
            variables_[name] = std::move(expr);
        }

        // Convenience: set a variable to a numeric value
        void set(const std::string& name, double value) {
            variables_[name] = std::make_unique<Number>(value);
        }

        // Get the stored expression for a variable (throws if not found)
        const Expr& get_expr(const std::string& name) const {
            auto it = variables_.find(name);
            if (it == variables_.end()) {
                throw std::runtime_error("undefined variable: " + name);
            }
            return *it->second;
        }

        // Evaluate a variable to a numeric value (resolves references).
        // Throws UndefinedVariableError if unresolved variables remain.
        // Throws CircularDependencyError on cycles.
        double get(const std::string& name) const {
            auto it = variables_.find(name);
            if (it == variables_.end()) {
                throw std::runtime_error("undefined variable: " + name);
            }
            std::unordered_set<std::string> visited;
            visited.insert(name);
            double val = resolve_(*it->second, visited);
            return val;
        }

        // Try to evaluate a variable numerically. Returns false if symbolic
        // (contains undefined sub-variables).
        bool try_get(const std::string& name, double& out) const {
            try {
                out = get(name);
                return true;
            } catch (...) {
                return false;
            }
        }

        // Check if variable exists
        bool has(const std::string& name) const {
            return variables_.find(name) != variables_.end();
        }

        // Check if a variable can be fully evaluated numerically
        bool is_numeric(const std::string& name) const {
            double dummy;
            return try_get(name, dummy);
        }

        // Remove a variable
        bool unset(const std::string& name) {
            return variables_.erase(name) > 0;
        }

        // Clear all variables
        void                     clear() { variables_.clear(); }

        // Get all variable names
        std::vector<std::string> all_names() const {
            std::vector<std::string> names;
            names.reserve(variables_.size());
            for (const auto& pair : variables_) {
                names.push_back(pair.first);
            }
            return names;
        }

        // Get all variables as map (for iteration)
        const std::unordered_map<std::string, ExprPtr>& all() const {
            return variables_;
        }

        // Get all variables that can be evaluated as numeric values.
        // Used for backward-compatible serialization.
        std::unordered_map<std::string, double> all_numeric() const {
            std::unordered_map<std::string, double> result;
            for (const auto& [name, _] : variables_) {
                double val;
                if (try_get(name, val)) {
                    result[name] = val;
                }
            }
            return result;
        }

        // Get all variables as expression strings (for display/serialization)
        std::unordered_map<std::string, std::string> all_as_strings() const {
            std::unordered_map<std::string, std::string> result;
            for (const auto& [name, expr] : variables_) {
                result[name] = expr->to_string();
            }
            return result;
        }

        // Number of defined variables
        size_t size() const { return variables_.size(); }

        bool   empty() const { return variables_.empty(); }
    };

} // namespace math_solver

#endif
