#pragma once

#include <string>

namespace math_solver {

    // Token types for the command-line interface lexer.
    // - Command: CLI commands prefixed with ':' (e.g., :set).
    // - Flag: Option flags prefixed with '-' or '--'.
    // - Word: Unquoted identifiers or literals.
    // - QuotedString: Used for arguments requiring whitespace (e.g., file
    // paths).
    // - Eof: Sentinel for end-of-input; must be handled explicitly by
    // consumers.
    //
    // Invariant: Each token type is mutually exclusive and fully partitions the
    // input.
    enum class CommandTokenType { Command, Flag, Word, QuotedString, Eof };

    struct CommandToken {
        CommandTokenType type;
        std::string
               value; // For QuotedString, quotes are stripped by the lexer.
        size_t start; // Byte offset in the original input (inclusive).
        size_t end;   // Byte offset in the original input (exclusive).
        //
        // Invariant: 0 <= start <= end <= input.size().
        // The [start, end) range must correspond exactly to the token's span in
        // the input. Consumers may rely on this for error reporting and
        // diagnostics.

        CommandToken(CommandTokenType   t,
                     const std::string& v,
                     size_t             s,
                     size_t             e)
            : type(t), value(v), start(s), end(e) {}

        // Returns true if the token is of the specified type.
        // Used by parser routines to dispatch on token kind.
        bool is(CommandTokenType t) const { return type == t; }
    };

    // Returns a stable, human-readable name for each token type.
    // Used for diagnostics and debugging output.
    // Must be kept in sync with CommandTokenType.
    inline const char* command_token_type_name(CommandTokenType type) {
        switch (type) {
        case CommandTokenType::Command:
            return "command";
        case CommandTokenType::Flag:
            return "flag";
        case CommandTokenType::Word:
            return "word";
        case CommandTokenType::QuotedString:
            return "quoted string";
        case CommandTokenType::Eof:
            return "end of input";
        default:
            return "unknown";
        }
    }

} // namespace math_solver
