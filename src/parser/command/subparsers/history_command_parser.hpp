#pragma once

#include "icommand_subparser.hpp"
#include "lexer/command/token_stream.hpp"

namespace math_solver {

    // Handles parsing of :history commands, which multiplex several subcommands
    // (show, search, save, clear) with overlapping syntactic forms.
    //
    // - Ambiguity: The grammar is intentionally permissive to allow both
    //   positional and keyword-based forms. Care must be taken to disambiguate
    //   between numeric arguments and subcommand keywords.
    //
    // - Invariant: All parsing must leave the stream in a consistent state,
    //   regardless of early returns or parse failures, to avoid corrupting
    //   subsequent command parsing.
    //
    // - Performance: Parsing is expected to be fast, as this is on the
    //   interactive command path. Avoid unnecessary allocations or lookahead.
    //
    // - Interactions: The parser must not assume anything about the underlying
    //   history storage or command execution semantics; it is strictly a
    //   syntactic dispatcher.
    class HistoryCommandParser : public ICommandSubparser {
        public:
        Result<CommandPtr> parse(ITokenStream& stream) override;

        private:
        // Parses the "show" variant, possibly with range or limit arguments.
        // Assumes `first` is the first numeric argument already parsed.
        Result<CommandPtr> parse_show(ITokenStream& stream, int first);

        // Parses the "save" subcommand, handling both full and range forms.
        Result<CommandPtr> parse_save(ITokenStream& stream);

        // Parses the "search" subcommand, extracting the pattern argument.
        Result<CommandPtr> parse_search(ITokenStream& stream);
    };

} // namespace math_solver