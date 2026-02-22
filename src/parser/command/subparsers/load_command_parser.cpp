#include "load_command_parser.hpp"
#include "ast/command/load_command.hpp"
#include "core/error.hpp"

namespace math_solver {

    CommandPtr LoadCommandParser::parse(ITokenStream& stream) {
        std::string raw = stream.raw_input();
        stream.advance(); // consume ":load" token

        // Defensive: Require at least one argument after ":load".
        if (stream.is_eof()) {
            throw MathException(
                errors::parse("missing filepath for ':load' command",
                              find_token_span(raw, ":load"), raw))
                .with_help("Usage: :load [flags] <filepath>");
        }

        LoadCommand::Flags flags;
        std::string        filepath;

        // Invariant: Only one positional argument (filepath) is allowed.
        // Flags must precede or follow the filepath; order is not enforced.
        while (!stream.is_eof()) {
            const auto& token     = stream.peek();
            std::string token_val = token.value;

            // Flag parsing: All flags must be recognized.
            // Unknown flags are treated as hard errors to avoid silent
            // misconfiguration.
            if (token_val.rfind("--", 0) == 0) {
                stream.advance(); // consume flag

                if (token_val == "--dry-run") {
                    flags.dry_run = true;
                } else if (token_val == "--silent") {
                    flags.silent = true;
                } else if (token_val == "--strict") {
                    flags.strict = true;
                } else if (token_val == "--no-rollback") {
                    flags.no_rollback = true;
                } else if (token_val == "--env") {
                    // Correctness: "--env" must be followed by a valid
                    // environment name.
                    if (stream.is_eof()) {
                        Error err = errors::parse(
                            "expected environment name after '--env'",
                            find_token_span(raw, "--env"), raw);
                        err.help = "example: :load script.msl --env production";
                        throw MathException(err);
                    }
                    flags.env = stream.advance().value;
                } else {
                    // Strict: Disallow unknown flags to prevent accidental
                    // misuse.
                    Error err =
                        errors::parse("unknown flag '" + token_val + "'",
                                      find_token_span(raw, token_val), raw);
                    err.help = "available flags: --dry-run, --silent, "
                               "--strict, --no-rollback, --env";
                    throw MathException(err);
                }
            } else {
                // Only a single filepath is permitted; extra positional
                // arguments are rejected.
                if (filepath.empty()) {
                    filepath = stream.advance().value;
                } else {
                    Error err = errors::parse(
                        "unexpected positional argument '" + token_val + "'",
                        find_token_span(raw, token_val), raw);
                    err.inline_label = "extra argument";
                    err.help = "only one filepath is supported per command.";
                    throw MathException(err);
                }
            }
        }

        // Final check: At least one filepath must be provided.
        if (filepath.empty()) {
            Error err = errors::parse("no filepath provided",
                                      find_token_span(raw, ":load"), raw);
            err.help  = "you must specify a file to load. Example: :load "
                        "script.msl";
            throw MathException(err);
        }

        // Returns a LoadCommand instance with parsed flags and filepath.
        return std::make_unique<LoadCommand>(filepath, flags, raw);
    }

} // namespace math_solver