#pragma once

#include "config/config.hpp"
#include "runtime/context/context.hpp"

#include <string>

namespace math_solver {

    // Entry for interactive REPL session.
    //
    // - Assumes `g_config` and `g_ctx` are fully initialized and remain valid
    // for the duration of the call.
    // - `g_current_env` must reference a valid environment name on entry; may
    // be mutated to reflect user-driven environment switches.
    // - Returns process exit code; nonzero signals unrecoverable error or
    // explicit shutdown.
    //
    // Invariants:
    // - No global state is mutated except via the provided
    // context/config/environment references.
    // - All session-local state is transient; persistent state must be managed
    // via `g_ctx`.
    //
    // Performance:
    // - Designed to minimize blocking on I/O; responsiveness is critical for
    // user experience.
    //
    // Interactions:
    // - May trigger side effects in the runtime context (e.g., environment
    // changes, I/O).
    // - Assumes external initialization and teardown of global state.
    int run_repl(Config&            g_config,
                 Context&           g_ctx,
                 std::string&       g_current_env,
                 const std::string& version);

} // namespace math_solver