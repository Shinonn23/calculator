#include "var_command_parser.hpp"
#include "ast/command/var_command.hpp"
#include "core/span.hpp"
#include "diagnostics/kinds/command_errors.hpp"

#include <cctype>

namespace math_solver {

    static std::string extract_trailing_flag(const std::string& s) {
        size_t e = s.find_last_not_of(" \t\r\n");
        if (e == std::string::npos)
            return "";
        size_t b = s.find_last_of(" \t\r\n", e);
        b        = (b == std::string::npos) ? 0 : b + 1;
        if (e - b + 1 >= 2 && s[b] == '-' &&
            (std::isalpha(static_cast<unsigned char>(s[b + 1])) ||
             s[b + 1] == '-'))
            return s.substr(b, e - b + 1);
        return "";
    }

    // Parses a variable command from the token stream.
    //
    // Invariant: The stream is expected to be positioned at either ":set" or
    // ":unset". The parser distinguishes between set/unset actions based on the
    // first token.
    //
    // If the stream is at EOF after the action, we synthesize a VarCommand with
    // an empty variable name. This is relied upon by downstream consumers to
    // handle incomplete commands gracefully.
    //
    // For ":set", if the next token is a recognized math action ("solve",
    // "expand", "factor"), we treat it as a payload and consume the remainder
    // as its argument. Otherwise, the payload is empty.
    //
    // Note: The parser does not validate the variable name or payload contents
    // here; this is deferred to later passes.
    Result<CommandPtr> VarCommandParser::parse(ITokenStream& stream) {
        bool is_set = (stream.peek().value == ":set");
        stream.advance();
        if (stream.is_eof()) {
            return Result<CommandPtr>::ok(std::make_unique<VarCommand>(
                is_set ? VarCommand::Action::Set : VarCommand::Action::Unset,
                std::vector<std::string>(), stream.raw_input()));
        }

        std::vector<std::string> var_names = {};
        Span                     first_var_span;

        while ((stream.peek_is(CommandTokenType::Word))) {
            auto tok = stream.peek();
            if (var_names.empty())
                first_var_span = Span{tok.start, tok.end};
            var_names.push_back(tok.value);
            stream.advance();
            if (stream.peek_is(CommandTokenType::Comma)) {
                stream.advance();
            } else {
                break;
            }
        }

        auto var_cmd = std::make_unique<VarCommand>(
            is_set ? VarCommand::Action::Set : VarCommand::Action::Unset,
            var_names, stream.raw_input());
        var_cmd->set_var_name_span(first_var_span);
        if (is_set) {
            // Only attach a math action payload if the next token is a
            // recognized action. This avoids misinterpreting arbitrary input as
            // a math action.
            //
            // In both branches a quoted-string token is accepted as the
            // expression payload (quotes already stripped by the lexer).
            // This allows:  :set x "expr with spaces"
            //               :set y solve "x^2 - 4 = 0"
            if (stream.peek_is(CommandTokenType::Word) &&
                (stream.peek().value == "solve" ||
                 stream.peek().value == "expand" ||
                 stream.peek().value == "factor")) {
                std::string math_action = stream.peek().value;
                stream.advance();
                std::string payload;
                if (stream.peek_is(CommandTokenType::QuotedString)) {
                    payload = stream.advance().value;
                } else {
                    payload = stream.consume_remaining();
                    if (auto flag = extract_trailing_flag(payload);
                        !flag.empty())
                        return Result<CommandPtr>::err(
                            errors::trailing_flag_after_expr(
                                flag, stream.raw_input()));
                }
                var_cmd->set_payload(math_action, payload);
            } else {
                std::string payload;
                if (stream.peek_is(CommandTokenType::QuotedString)) {
                    payload = stream.advance().value;
                } else {
                    payload = stream.consume_remaining();
                    if (auto flag = extract_trailing_flag(payload);
                        !flag.empty())
                        return Result<CommandPtr>::err(
                            errors::trailing_flag_after_expr(
                                flag, stream.raw_input()));
                }
                var_cmd->set_payload("", payload);
            }
        }
        return Result<CommandPtr>::ok(std::move(var_cmd));
    }

} // namespace math_solver
