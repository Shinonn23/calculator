#pragma once

#include "config/config.hpp"
#include "runtime/context/context.hpp"

#include <string>

namespace math_solver {

    // Entry point for the interactive REPL subsystem.
    // - Assumes `g_config` and `g_ctx` are fully initialized by the caller.
    // - `g_current_env` must reference a valid environment name; the REPL may
    // mutate it.
    // - Returns process exit code; nonzero indicates unrecoverable error or
    // shutdown request.
    // - REPL state is not persisted across invocations; all session state is in
    // `g_ctx`.
    // - Interacts with the runtime context and may trigger side effects (I/O,
    // env changes).
    // - Performance: Designed for responsiveness; blocking operations should be
    // minimized.
    // - Invariants: No global state is mutated outside of the provided
    // context/config.
    int run_repl(Config& g_config, Context& g_ctx, std::string& g_current_env);

} // namespace math_solver