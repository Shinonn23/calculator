#ifndef CONTEXT_H
#define CONTEXT_H

#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace math_solver {
    class Context {
        private:
        std::unordered_map<std::string, double> variables_;

        public:
        Context() = default;

        // Set a variable value
        void set(const std::string& name, double value) {
            variables_[name] = value;
        }

        // Get a variable value (throws if not found)
        double get(const std::string& name) const {
            auto it = variables_.find(name);
            if (it == variables_.end()) {
                throw std::runtime_error("undefined variable: " + name);
            }
            return it->second;
        }

        // Check if variable exists
        bool has(const std::string& name) const {
            return variables_.find(name) != variables_.end();
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
        const std::unordered_map<std::string, double>& all() const {
            return variables_;
        }

        // Number of defined variables
        size_t size() const { return variables_.size(); }

        bool   empty() const { return variables_.empty(); }
    };

} // namespace math_solver

#endif
