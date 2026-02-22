#include "runtime.hpp"
#include "context/resolver.hpp"
#include "parser/math/math_parser.hpp"
#include "ui/color.hpp"
#include "ui/suggestions.hpp"
#include <iostream>

using namespace std;

namespace math_solver {

    void Runtime::save_environment() {
        // Persist the current environment state before any mutation.
        // This is relied upon by load_environment to avoid accidental loss of
        // user state.
        config_.save_env_variables(current_env_, context_.all_as_strings());
    }

    bool Runtime::load_environment(const std::string& env_name) {
        if (!config_.env_exists(env_name)) {
            // Defensive: If the requested environment is missing, emit
            // diagnostics and attempt to provide a suggestion. This is
            // user-facing, so keep output concise.
            auto envs  = config_.list_envs();
            auto match = suggest(env_name, envs);
            if (match) {
                cout << ansi::dim << "  Did you mean " << ansi::reset
                     << ansi::bold << *match << ansi::reset << ansi::dim << "?"
                     << ansi::reset << "\n";
            }
            return false;
        }

        // Save the current environment before switching, to avoid silent data
        // loss.
        save_environment();

        context_.clear();
        const auto& env = config_.get_env(env_name);

        // Restore all variables from the target environment.
        // Parsing failures are tolerated (e.g., due to legacy or malformed
        // data). If parsing as an expression fails, fallback to parsing as a
        // double. Variables that fail both are silently skipped to maximize
        // robustness.
        for (const auto& [name, expr_str] : env.variables) {
            try {
                Parser parser(expr_str);
                auto   parse_result = parser.parse();
                if (!parse_result)
                    throw std::runtime_error("parse failed");
                context_.set(name, std::move(*parse_result));
            } catch (...) {
                try {
                    double val = std::stod(expr_str);
                    context_.set(name, val);
                } catch (...) {
                    // Intentionally ignored: variable is omitted from context.
                }
            }
        }
        current_env_ = env_name;
        return true;
    }

    double Runtime::evaluate(const std::string& var_name) const {
        // Delegates to Resolver for variable evaluation.
        // Assumes context_ is up-to-date and consistent.
        return Resolver::evaluate_variable(var_name, context_);
    }

} // namespace math_solver