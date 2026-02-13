#ifndef ENVIRONMENT_H
#define ENVIRONMENT_H
#include "../eval/context.hpp"
#include "color.hpp"
#include "config.hpp"
#include "suggest.hpp"
#include <iostream>
#include <string>


using namespace std;

namespace math_solver {

    // Save current context variables into the current environment in config
    void save_current_env_to_config(Config&       g_config,
                                    const string& g_current_env,
                                    Context&      g_ctx) {
        g_config.save_env_variables(g_current_env, g_ctx.all());
    }

    // Load an environment's variables into the context
    void load_env_into_context(const string& env_name, Context& g_ctx,
                               Config& g_config, string& g_current_env) {
        if (!g_config.env_exists(env_name)) {
            cout << ansi::red << "  Error: " << ansi::reset << "environment '"
                 << env_name << "' not found\n";
            auto envs  = g_config.list_envs();
            auto match = suggest(env_name, envs);
            if (match) {
                cout << ansi::dim << "  Did you mean " << ansi::reset
                     << ansi::bold << *match << ansi::reset << ansi::dim << "?"
                     << ansi::reset << "\n";
            }
            return;
        }
        // Save current state first
        save_current_env_to_config(g_config, g_current_env, g_ctx);

        // Clear and load new env
        g_ctx.clear();
        const auto& env = g_config.get_env(env_name);
        for (const auto& [name, value] : env.variables) {
            g_ctx.set(name, value);
        }
        g_current_env = env_name;
    }
} // namespace math_solver

#endif