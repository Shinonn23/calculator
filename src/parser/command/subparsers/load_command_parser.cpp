#include "load_command_parser.hpp"
#include "ast/command/load_command.hpp"
#include "core/error.hpp"

namespace math_solver {

    CommandPtr LoadCommandParser::parse(ITokenStream& stream) {
        std::string raw = stream.raw_input();
        stream.advance(); // consume ":load" token

        // Defensive: Require at least one argument after ":load".
        if (stream.is_eof()) {
            throw ParseError("missing filepath for ':load' command",
                             find_token_span(raw, ":load"),
                             raw)
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
                } else if (token_val == "--env") {
                    // Correctness: "--env" must be followed by a valid
                    // environment name.
                    if (stream.is_eof()) {
                        throw ParseError(
                            "expected environment name after '--env'",
                            find_token_span(raw, "--env"),
                            raw)
                            .with_help(
                                "example: :load script.msl --env production");
                    }
                    flags.env = stream.advance().value;
                } else {
                    // Strict: Disallow unknown flags to prevent accidental
                    // misuse.
                    throw ParseError("unknown flag '" + token_val + "'",
                                     find_token_span(raw, token_val),
                                     raw)
                        .with_help(
                            "available flags: --dry-run, --silent, --env");
                }
            } else {
                // Only a single filepath is permitted; extra positional
                // arguments are rejected.
                if (filepath.empty()) {
                    filepath = stream.advance().value;
                } else {
                    throw ParseError("unexpected positional argument '" +
                                         token_val + "'",
                                     find_token_span(raw, token_val),
                                     raw)
                        .with_label("extra argument")
                        .with_help(
                            "only one filepath is supported per command.");
                }
            }
        }

        // Final check: At least one filepath must be provided.
        if (filepath.empty()) {
            throw ParseError(
                "no filepath provided", find_token_span(raw, ":load"), raw)
                .with_help("you must specify a file to load. Example: :load "
                           "script.msl");
        }

        // Returns a LoadCommand instance with parsed flags and filepath.
        return std::make_unique<LoadCommand>(filepath, flags, raw);
    }

} // namespace math_solver