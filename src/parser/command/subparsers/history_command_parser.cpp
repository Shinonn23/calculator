#include "history_command_parser.hpp"
#include "ast/command/history_command.hpp"
#include "utils/history_range.hpp"

namespace math_solver {

    CommandPtr HistoryCommandParser::parse(ITokenStream& stream) {
        stream.advance(); // consume ":history" token

        // Flag parsing must precede positional arguments. Unknown flags are
        // mapped to None to preserve forward compatibility; downstream logic
        // must handle None explicitly.
        std::vector<HistoryCommand::Flag> flags;
        while (!stream.is_eof() && stream.peek().value.rfind("--", 0) == 0) {
            const std::string& f = stream.peek().value;
            if (f == "--errors")
                flags.push_back(HistoryCommand::Flag::Errors);
            else if (f == "--success")
                flags.push_back(HistoryCommand::Flag::Success);
            else if (f == "--info")
                flags.push_back(HistoryCommand::Flag::Info);

            else if (f == "--warning")
                flags.push_back(HistoryCommand::Flag::Warning);

            else
                flags.push_back(HistoryCommand::Flag::None);
            stream.advance();
        }

        if (stream.is_eof()) {
            // Default to showing the last 20 entries if no arguments are
            // present. This is a UX-driven default; downstream must not assume
            // this limit is always set.
            auto cmd = std::make_unique<HistoryCommand>(
                HistoryCommand::Action::Show, stream.raw_input());
            cmd->set_limit(20);
            cmd->set_flags(flags);
            return cmd;
        }

        const std::string& word = stream.peek().value;

        if (word == "clear") {
            // "clear" is a hard reset; all history is dropped.
            // No flags or arguments are permitted after "clear".
            stream.advance();
            return std::make_unique<HistoryCommand>(
                HistoryCommand::Action::Clear, stream.raw_input());
        }

        if (word == "search") {
            // "search" is delegated; pattern parsing is handled in
            // parse_search. No flags are propagated to search.
            stream.advance();
            return parse_search(stream);
        }

        if (word == "save") {
            // "save" expects a filepath and optional selector.
            // Selector parsing is deferred to parse_save.
            stream.advance();
            return parse_save(stream);
        }

        if (word == "all") {
            // "all" disables the entry limit (limit=0).
            // This is interpreted as "show all history".
            stream.advance();
            auto cmd = std::make_unique<HistoryCommand>(
                HistoryCommand::Action::Show, stream.raw_input());
            cmd->set_limit(0);
            cmd->set_flags(flags);
            return cmd;
        }

        // Accepts range syntax only if token starts with a digit and contains a
        // dash. Defensive: avoids misinterpreting non-range tokens as ranges.
        if (word.find('-') != std::string::npos &&
            std::isdigit(static_cast<unsigned char>(word[0]))) {
            std::string range_str = word;
            stream.advance();

            auto cmd = std::make_unique<HistoryCommand>(
                HistoryCommand::Action::ShowRange, stream.raw_input());
            cmd->set_flags(flags);

            // Range parsing is fallible; errors are deferred to downstream
            // error handling.
            try {
                auto range = HistoryRange::parse(range_str);
                cmd->set_range(range);
            } catch (...) {
                // Range is intentionally left unset; error will be surfaced
                // later.
            }
            return cmd;
        }

        // Accepts positional numeric arguments only if the first token is
        // numeric. This branch is mutually exclusive with the range branch
        // above.
        if (std::isdigit(static_cast<unsigned char>(word[0]))) {
            int first = std::stoi(word);
            stream.advance();
            return parse_show(stream, first);
        }

        // Fallback: treat as default show command with limit=20.
        // This path is reached for unknown or missing arguments.
        auto cmd = std::make_unique<HistoryCommand>(
            HistoryCommand::Action::Show, stream.raw_input());
        cmd->set_limit(20);
        cmd->set_flags(flags);
        return cmd;
    }

    CommandPtr HistoryCommandParser::parse_show(ITokenStream& stream,
                                                int           first) {
        // All flags after the first positional argument are parsed here.
        // Invariant: flags must follow the first positional argument.
        std::vector<HistoryCommand::Flag> flags;
        while (!stream.is_eof() && stream.peek().value.rfind("--", 0) == 0) {
            const std::string& f = stream.peek().value;
            if (f == "--errors")
                flags.push_back(HistoryCommand::Flag::Errors);
            else if (f == "--success")
                flags.push_back(HistoryCommand::Flag::Success);
            else if (f == "--info")
                flags.push_back(HistoryCommand::Flag::Info);
            else if (f == "--warning")
                flags.push_back(HistoryCommand::Flag::Warning);
            else
                flags.push_back(HistoryCommand::Flag::None);
            stream.advance();
        }

        // Accepts either ":history <n> <m>" (space-separated) or a single range
        // token. If a second numeric token is present, treat as explicit range.
        // Invariant: first <= second after swap.
        if (!stream.is_eof() &&
            std::isdigit(static_cast<unsigned char>(stream.peek().value[0]))) {
            int second = std::stoi(stream.peek().value);
            stream.advance();

            if (first > second) {
                std::swap(first, second);
            }

            std::vector<int> range;
            for (int i = first; i <= second; ++i)
                range.push_back(i);

            auto cmd = std::make_unique<HistoryCommand>(
                HistoryCommand::Action::ShowRange, stream.raw_input());
            cmd->set_range(range);
            cmd->set_flags(flags);
            return cmd;
        }

        // Defensive: If a range token (e.g., "9999-10000") was not handled
        // above, this branch is unreachable if upstream logic is correct.

        // Default: treat as show last <n> entries.
        auto cmd = std::make_unique<HistoryCommand>(
            HistoryCommand::Action::Show, stream.raw_input());
        cmd->set_limit(first);
        cmd->set_flags(flags);
        return cmd;
    }

    CommandPtr HistoryCommandParser::parse_save(ITokenStream& stream) {
        if (stream.is_eof()) {
            return std::make_unique<HistoryCommand>(
                HistoryCommand::Action::Save, stream.raw_input());
        }

        std::string filepath = stream.peek().value;
        stream.advance();

        auto cmd = std::make_unique<HistoryCommand>(
            HistoryCommand::Action::Save, stream.raw_input());
        cmd->set_filepath(filepath);

        if (!stream.is_eof()) {
            std::string full_selector = stream.consume_remaining();

            full_selector.erase(
                std::remove(full_selector.begin(), full_selector.end(), ' '),
                full_selector.end());

            if (!full_selector.empty() &&
                std::isdigit(static_cast<unsigned char>(full_selector[0]))) {
                try {
                    auto range = HistoryRange::parse(full_selector);
                    cmd->set_range(range);
                } catch (...) {
                }
            }
        }

        return cmd;
    }

    CommandPtr HistoryCommandParser::parse_search(ITokenStream& stream) {
        auto cmd = std::make_unique<HistoryCommand>(
            HistoryCommand::Action::Search, stream.raw_input());

        // All remaining input is treated as the search pattern.
        // No further token validation is performed here.
        if (!stream.is_eof()) {
            cmd->set_pattern(stream.consume_remaining());
        }

        return cmd;
    }

} // namespace math_solver
