#pragma once

#include "subparsers/icommand_subparser.hpp"
#include <memory>
#include <string>
#include <unordered_map>

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

} // namespace math_solver
