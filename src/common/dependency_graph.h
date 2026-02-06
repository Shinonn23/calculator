#ifndef DEPENDENCY_GRAPH_H
#define DEPENDENCY_GRAPH_H

#include <algorithm>
#include <queue>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace math_solver {

    // ================================================================
    // DependencyGraph — first-class module for tracking variable
    // dependencies.
    //
    // Used by: Context (cycle detection), cmd_set (overwrite warnings),
    //          Domain system (constraint propagation — future).
    //
    // edges_[A] = {B, C} means: A depends on B and C
    // (i.e., "set A B + C" creates edges A → B, A → C)
    // ================================================================
    class DependencyGraph {
        private:
        // Forward edges: variable → set of variables it depends on
        std::unordered_map<std::string, std::set<std::string>> edges_;

        // Reverse edges: variable → set of variables that depend on it
        std::unordered_map<std::string, std::set<std::string>> reverse_edges_;

        // DFS cycle detection helper
        bool has_cycle_from(const std::string& node, const std::string& target,
                            std::unordered_set<std::string>& visited) const {
            if (node == target)
                return true;
            if (visited.count(node))
                return false;
            visited.insert(node);

            auto it = edges_.find(node);
            if (it == edges_.end())
                return false;

            for (const auto& dep : it->second) {
                if (has_cycle_from(dep, target, visited))
                    return true;
            }
            return false;
        }

        // Transitive closure helper
        void
        collect_transitive(const std::string&               node,
                           std::unordered_set<std::string>& visited) const {
            auto it = edges_.find(node);
            if (it == edges_.end())
                return;
            for (const auto& dep : it->second) {
                if (visited.insert(dep).second) {
                    collect_transitive(dep, visited);
                }
            }
        }

        public:
        DependencyGraph() = default;

        // ── Mutators ────────────────────────────────────────────

        // Register a variable with its direct dependencies.
        // Replaces any previous entry for this variable.
        void add_variable(const std::string&           name,
                          const std::set<std::string>& deps) {
            // Remove old reverse edges for this variable
            auto old_it = edges_.find(name);
            if (old_it != edges_.end()) {
                for (const auto& old_dep : old_it->second) {
                    auto rev_it = reverse_edges_.find(old_dep);
                    if (rev_it != reverse_edges_.end()) {
                        rev_it->second.erase(name);
                        if (rev_it->second.empty()) {
                            reverse_edges_.erase(rev_it);
                        }
                    }
                }
            }

            // Set new forward edges
            edges_[name] = deps;

            // Build new reverse edges
            for (const auto& dep : deps) {
                reverse_edges_[dep].insert(name);
            }
        }

        // Remove a variable from the graph.
        void remove_variable(const std::string& name) {
            // Remove forward edges
            auto it = edges_.find(name);
            if (it != edges_.end()) {
                for (const auto& dep : it->second) {
                    auto rev_it = reverse_edges_.find(dep);
                    if (rev_it != reverse_edges_.end()) {
                        rev_it->second.erase(name);
                        if (rev_it->second.empty()) {
                            reverse_edges_.erase(rev_it);
                        }
                    }
                }
                edges_.erase(it);
            }

            // Remove reverse edges (other variables depending on this one)
            // Note: we don't remove the dependents — they still depend on
            // it, it's just undefined now.
            reverse_edges_.erase(name);
        }

        // Clear the entire graph.
        void clear() {
            edges_.clear();
            reverse_edges_.clear();
        }

        // ── Queries ─────────────────────────────────────────────

        // Check whether assigning `name` with deps `new_deps` would
        // create a cycle. Does NOT modify the graph.
        bool would_cycle(const std::string&           name,
                         const std::set<std::string>& new_deps) const {
            // Direct self-reference
            if (new_deps.count(name))
                return true;

            // Check if any dependency transitively reaches back to `name`
            // We simulate adding the edge and check reachability
            std::unordered_set<std::string> visited;
            for (const auto& dep : new_deps) {
                if (has_cycle_from(dep, name, visited))
                    return true;
            }
            return false;
        }

        // Get direct dependencies of a variable.
        std::set<std::string> dependencies_of(const std::string& name) const {
            auto it = edges_.find(name);
            if (it == edges_.end())
                return {};
            return it->second;
        }

        // Get variables that directly depend on `name`.
        std::set<std::string> dependents_of(const std::string& name) const {
            auto it = reverse_edges_.find(name);
            if (it == reverse_edges_.end())
                return {};
            return it->second;
        }

        // Get all transitive dependencies (full closure).
        std::set<std::string> transitive_deps(const std::string& name) const {
            std::unordered_set<std::string> visited;
            collect_transitive(name, visited);
            return std::set<std::string>(visited.begin(), visited.end());
        }

        // Topological sort of all nodes. Returns empty if cycle exists.
        std::vector<std::string> topological_order() const {
            // Compute in-degree
            std::unordered_map<std::string, int> in_degree;
            std::unordered_set<std::string>      all_nodes;
            for (const auto& [node, deps] : edges_) {
                all_nodes.insert(node);
                if (in_degree.find(node) == in_degree.end())
                    in_degree[node] = 0;
                for (const auto& dep : deps) {
                    all_nodes.insert(dep);
                    in_degree[dep]++; // dep is depended upon by node
                }
            }
            // Note: in this graph, edges point FROM dependent TO dependency
            // For topological sort, we want dependencies first.
            // So we reverse: in_degree counts how many things depend on a
            // node.
            // Actually, let's use reverse: we want leaves (no dependencies)
            // first.
            std::unordered_map<std::string, int> out_degree;
            for (const auto& node : all_nodes) {
                auto it          = edges_.find(node);
                out_degree[node] = (it != edges_.end())
                                       ? static_cast<int>(it->second.size())
                                       : 0;
            }

            std::queue<std::string> q;
            for (const auto& node : all_nodes) {
                if (out_degree[node] == 0)
                    q.push(node);
            }

            std::vector<std::string> result;
            while (!q.empty()) {
                std::string node = q.front();
                q.pop();
                result.push_back(node);

                // For each variable that depends on this node
                auto rev_it = reverse_edges_.find(node);
                if (rev_it != reverse_edges_.end()) {
                    for (const auto& dependent : rev_it->second) {
                        out_degree[dependent]--;
                        if (out_degree[dependent] == 0) {
                            q.push(dependent);
                        }
                    }
                }
            }

            if (result.size() != all_nodes.size()) {
                return {}; // Cycle detected
            }
            return result;
        }

        // Check if variable exists in the graph.
        bool has(const std::string& name) const {
            return edges_.find(name) != edges_.end();
        }

        // Number of variables in the graph.
        size_t size() const { return edges_.size(); }

        bool   empty() const { return edges_.empty(); }
    };

} // namespace math_solver

#endif
