#pragma once

#include "subparsers/icommand_subparser.hpp"
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace math_solver {

    // Registry mapping command names to their corresponding subparser
    // instances.
    //
    // Invariant: Each key is a unique command identifier; values are
    // unique_ptrs owning the subparser implementation. Subparsers are expected
    // to be stateless or reentrant, as registry lifetime is tied to the parser
    // infrastructure.
    //
    // Performance: Lookup cost is critical for command dispatch; unordered_map
    // chosen for O(1) average-case lookup. Registry is constructed once at
    // startup.
    using SubparserRegistry =
        std::unordered_map<std::string, std::unique_ptr<ICommandSubparser>>;

    // Constructs and returns the registry of available command subparsers.
    //
    // All subparsers must be registered here to participate in command parsing.
    // This function is the single point of extension for new command types.
    // Callers must not mutate the returned registry after construction.
    SubparserRegistry build_registry();

    // Returns the set of all registered command names (e.g. ":solve",
    // ":set"). Built once from build_registry() and cached as a static local.
    //
    // Use this as the single source of truth for command validation — avoids
    // duplicating the command list in callers such as the REPL highlighter.
    // Any command added to build_registry() is automatically reflected here.
    const std::unordered_set<std::string>& registered_command_names();

} // namespace math_solver
