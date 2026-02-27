#include "redo_command_parser.hpp"
#include "ast/command/redo_command.hpp"
#include "utils/history_range.hpp"

namespace math_solver {

    Result<CommandPtr> RedoCommandParser::parse(ITokenStream& stream) {
        stream.advance(); // always consume ":redo" token; parser invariant

        auto cmd = std::make_unique<RedoCommand>(stream.raw_input());

        // If no argument is present, default to redoing the last command.
        // This is the only case where an empty range is valid.
        if (stream.is_eof())
            return Result<CommandPtr>::ok(std::move(cmd));

        const std::string& selector = stream.peek().value;

        // Only numeric selectors are accepted for redo ranges.
        // Defensive: reject any non-numeric input to avoid misinterpreting
        // flags or malformed tokens. This ensures that only explicit user
        // intent is acted upon.
        if (selector.empty() ||
            !std::isdigit(static_cast<unsigned char>(selector[0])))
            return Result<CommandPtr>::ok(std::move(cmd));

        stream.advance();

        // Pass INT_MAX as the upper bound since the parser does not have access
        // to the actual history size. Responsibility for validating the
        // resolved range is deferred to the handler.
        auto range_r = HistoryRange::parse(selector);
        if (range_r) {
            cmd->set_range(*range_r);
        } else {
            return Result<CommandPtr>::err(range_r.error());
        }

        return Result<CommandPtr>::ok(std::move(cmd));
    }

} // namespace math_solver