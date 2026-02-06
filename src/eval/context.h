#ifndef CONTEXT_H
#define CONTEXT_H

#include "ast/expr.h"
#include "ast/number.h"
#include "ast/substitutor.h"
#include "common/dependency_graph.h"
#include "common/value.h"

#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace math_solver {
    class Context {
        private:
        // Variables now store expression ASTs (symbolic assignment).
        // Even numeric values are stored as Number nodes.
        std::unordered_map<std::string, ExprPtr> variables_;

        // First-class dependency graph for cycle detection and
        // dependency tracking.
        DependencyGraph                          graph_;

        // Session-level evaluation mode (set via :mode command)
        EvalMode                                 eval_mode_ = EvalMode::Numeric;

        public:
        Context() = default;

        // ── Store an expression (symbolic) ─────────────────────
        void set(const std::string& name, ExprPtr expr) {
            // Update dependency graph
            auto deps = free_variables(*expr);
            graph_.add_variable(name, deps);
            variables_[name] = std::move(expr);
        }

        // Convenience: store a numeric value (wraps in Number node)
        void set(const std::string& name, double value) {
            graph_.add_variable(name, {}); // No dependencies
            variables_[name] = std::make_unique<Number>(value);
        }

        // ── Retrieve the stored expression ─────────────────────
        // Returns nullptr if not found.
        const Expr* get_expr(const std::string& name) const {
            auto it = variables_.find(name);
            if (it == variables_.end())
                return nullptr;
            return it->second.get();
        }

        // Legacy: get a numeric value. Throws if not found.
        // Only works if the stored expression is a plain Number.
        // For symbolic expressions, use get_expr() instead.
        double get(const std::string& name) const {
            auto it = variables_.find(name);
            if (it == variables_.end()) {
                throw std::runtime_error("undefined variable: " + name);
            }
            // Try to extract numeric value from a Number node
            const auto* num = dynamic_cast<const Number*>(it->second.get());
            if (num) {
                return num->value();
            }
            throw std::runtime_error("variable '" + name +
                                     "' is symbolic, not numeric");
        }

        // Check if variable exists
        bool has(const std::string& name) const {
            return variables_.find(name) != variables_.end();
        }

        // Remove a variable
        bool unset(const std::string& name) {
            graph_.remove_variable(name);
            return variables_.erase(name) > 0;
        }

        // Clear all variables
        void clear() {
            variables_.clear();
            graph_.clear();
        }

        // Get all variable names
        std::vector<std::string> all_names() const {
            std::vector<std::string> names;
            names.reserve(variables_.size());
            for (const auto& pair : variables_) {
                names.push_back(pair.first);
            }
            return names;
        }

        // Get display string for a variable (expression to_string)
        std::string get_display(const std::string& name) const {
            auto it = variables_.find(name);
            if (it == variables_.end())
                return "";
            return it->second->to_string();
        }

        // ── Expression expansion ───────────────────────────────

        // Create a lookup function for the substitution visitor
        ExprLookupFn make_lookup() const {
            return [this](const std::string& name) -> const Expr* {
                return this->get_expr(name);
            };
        }

        // Expand all symbolic variables in an expression
        ExprPtr expand(const Expr& expr) const {
            return expand_expr(expr, make_lookup());
        }

        // ── Cycle Detection (via DependencyGraph) ──────────────

        // Check if storing expr under name would create a cycle.
        // Uses the DependencyGraph for O(V+E) detection instead of
        // trial-expansion.
        bool would_cycle(const std::string& name, const Expr& expr) const {
            auto deps = free_variables(expr);
            return graph_.would_cycle(name, deps);
        }

        // ── Dependency queries ─────────────────────────────────

        // Get variables that directly depend on `name`.
        std::set<std::string> dependents_of(const std::string& name) const {
            return graph_.dependents_of(name);
        }

        // Get all transitive dependencies of a variable.
        std::set<std::string> transitive_deps(const std::string& name) const {
            return graph_.transitive_deps(name);
        }

        // Access the dependency graph directly (read-only).
        const DependencyGraph& graph() const { return graph_; }

        // Number of defined variables
        size_t                 size() const { return variables_.size(); }

        bool                   empty() const { return variables_.empty(); }

        // ── Session eval mode ──────────────────────────────────
        EvalMode               eval_mode() const { return eval_mode_; }
        void                   set_eval_mode(EvalMode m) { eval_mode_ = m; }

        EvaluationConfig       eval_config() const {
            return EvaluationConfig{eval_mode_};
        }
    };

} // namespace math_solver

#endif
