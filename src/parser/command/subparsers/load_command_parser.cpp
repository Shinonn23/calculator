#include "load_command_parser.hpp"
#include "ast/command/load_command.hpp"
#include "core/error.hpp"

namespace math_solver {

    CommandPtr LoadCommandParser::parse(ITokenStream& stream) {
        stream.advance();

        if (stream.is_eof()) {
            // Defensive: ":load" must be followed by at least one argument.
            // This is enforced here to avoid propagating incomplete commands.
            throw ParseError(
                "expected env name after '--env'", Span(), stream.raw_input());
        }

        LoadCommand::Flags flags;

        // Flag parsing:
        // - All flags must precede the filepath.
        // - Unknown flags are tolerated for forward compatibility.
        // - "--env" requires a value; error if missing.
        // - No interleaving of flags and positional arguments is supported.
        while (!stream.is_eof() && stream.peek().value.rfind("--", 0) == 0) {

            std::string flag = stream.advance().value;

            if (flag == "--dry-run") {
                flags.dry_run = true;
            } else if (flag == "--silent") {
                flags.silent = true;
            } else if (flag == "--env") {
                if (stream.is_eof()) {
                    // "--env" must be followed by a value; this is a hard
                    // error.
                    throw ParseError("expected env name after '--env'",
                                     Span(),
                                     stream.raw_input());
                }
                flags.env = stream.advance().value;
            }
            // Unknown flags are ignored intentionally to avoid breaking
            // future extensions or out-of-tree consumers.
        }

        if (stream.is_eof()) {
            // At least one positional argument (filepath) is required.
            // This is a structural invariant for :load commands.
            throw ParseError(
                "expected filepath after flags", Span(), stream.raw_input());
        }

        // Accept both quoted and unquoted file paths.
        // No normalization or validation is performed here; deferred to later
        // stages.
        std::string filepath = stream.advance().value;

        // The raw input is preserved for diagnostics and reproducibility.
        // This enables precise error reporting and round-tripping.
        return std::make_unique<LoadCommand>(
            filepath, flags, stream.raw_input());
    }

} // namespace math_solver
