#pragma once

#include "config/config.hpp"
#include "context/context.hpp"
#include "diagnostics/result.hpp"
#include <string>

namespace math_solver {

    // Plain value snapshot of mutable runtime state.
    // Cheap to copy — Context and Config are both value types.
    struct RuntimeSnapshot {
        Context     ctx;
        Config      config;
        std::string current_env;
    };

    // Orchestrates state management and evaluation logic.
    //
    // Invariant: `config_` must outlive this Runtime instance.
    // `context_` is always kept in sync with the currently loaded environment.
    // All environment mutations must go through
    // save_environment/load_environment to avoid stale state or config
    // divergence.
    //
    // Performance: Context is stored by value to avoid pointer chasing during
    // frequent variable lookups and evaluation. This may incur copy costs
    // when switching environments, but avoids lifetime complexity.
    //
    // Interacts with: Config (persistent storage), Context (in-memory state).
    class Runtime {
        private:
        Context context_;
        Config&
            config_; // Non-owning; must remain valid for lifetime of Runtime.
        std::string current_env_;

        public:
        Runtime(Config& config, const std::string& default_env = "default")
            : config_(config), current_env_(default_env) {}

        Context&           get_context() { return context_; }
        const Context&     get_context() const { return context_; }

        const std::string& current_env() const { return current_env_; }

        // Persists the current Context state to the associated Config under
        // current_env_. Must be called after any mutation to Context that
        // should be durable.
        void               save_environment();

        // Loads variables from Config into Context for the given environment.
        // Returns false if the environment does not exist in Config.
        // On success, context_ is replaced wholesale; partial loads are not
        // supported.
        bool               load_environment(const std::string& env_name);

        // Evaluates the named variable using the current Context.
        // Assumes Context is up-to-date with the intended environment.
        // May throw or assert if var_name is not present.
        Result<double>     evaluate(const std::string& var_name) const;

        // ── Snapshot / Restore ────────────────────────────────────────────
        RuntimeSnapshot    snapshot() const {
            return {context_, config_, current_env_};
        }

        void restore(const RuntimeSnapshot& snap) {
            context_     = snap.ctx;
            config_      = snap.config;
            current_env_ = snap.current_env;
        }
    };

} // namespace math_solver
