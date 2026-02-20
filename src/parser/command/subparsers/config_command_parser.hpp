#pragma once

#include "icommand_subparser.hpp"

namespace math_solver {

    // Subparser for the 'config' command.
    //
    // This is responsible for parsing configuration-related commands from the
    // token stream. Assumes the stream is positioned at the start of a config
    // command; does not attempt to recover from malformed input (delegated to
    // higher-level error handling).
    //
    // Invariant: parse() must return a valid CommandPtr or propagate parse
    // errors upstream. This class is tightly coupled to ICommandSubparser and
    // is expected to be invoked only via the main command parser dispatch.
    class ConfigCommandParser : public ICommandSubparser {
        public:
        CommandPtr parse(ITokenStream& stream) override;
    };

} // namespace math_solver
