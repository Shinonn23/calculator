#include "command_parser.hpp"
#include "ast/command/math_command.hpp"
#include "ast/command/system_command.hpp"
#include "command_parser_registry.hpp"
#include "core/error.hpp"
#include "lexer/command/command_token_stream.hpp"
#include "subparsers/system_command_parser.hpp"
#include "utils/string_utils.hpp"

namespace math_solver {

    CommandParser::CommandParser(const std::string& input)
        : raw_input_(strip_comment(input)) {}

    Result<CommandPtr> CommandParser::parse() {
        CommandTokenStream stream(raw_input_);

        // Always synthesize an Evaluate command for empty input (after comment
        // stripping). This ensures downstream consumers never receive a null
        // command node, preserving invariants in the command dispatch pipeline.
        if (stream.is_eof()) {
            return Result<CommandPtr>::ok(std::make_unique<MathCommand>(
                MathCommand::Type::Evaluate, "", raw_input_));
        }

        // Colon-prefixed commands are dispatched via a static registry.
        // - Registry is constructed once per process; assumes immutability
        // after init.
        // - Unrecognized colon-prefixed commands intentionally fall back to
        // math evaluation,
        //   to avoid hard failures on unknown extensions or typos.
        // - This branch must precede legacy/system command handling to avoid
        // ambiguity.
        if (stream.peek_is(CommandTokenType::Command)) {
            std::string              cmd      = stream.peek().value;
            static SubparserRegistry registry = build_registry();
            auto                     it       = registry.find(cmd);
            if (it != registry.end()) {
                try {
                    return Result<CommandPtr>::ok(it->second->parse(stream));
                } catch (const MathException& e) {
                    return Result<CommandPtr>::err(e.error());
                } catch (const std::exception& e) {
                    auto t = stream.peek();
                    return Result<CommandPtr>::err(Error::make(
                        e.what(), "E0200", Span{t.start, t.end}, raw_input_));
                }
            }
            // Fallback: treat unknown colon-prefixed commands as math
            // expressions.
            return Result<CommandPtr>::ok(std::make_unique<SystemCommand>(
                SystemCommand::Type::Unknown, raw_input_));
        }

        // Legacy support for system commands without colon prefix.
        // - SystemCommandParser is static to avoid repeated construction.
        // - If parsing fails, fall through to math expression fallback.
        // - This branch is only taken if the input does not match a known
        // colon-prefixed command.
        {
            std::string                cmd = stream.peek().value;
            static SystemCommandParser sys_parser;
            try {
                auto sys = sys_parser.parse(stream);
                if (sys)
                    return Result<CommandPtr>::ok(std::move(sys));
            } catch (const MathException& e) {
                return Result<CommandPtr>::err(e.error());
            } catch (const std::exception& e) {
                auto t = stream.peek();
                return Result<CommandPtr>::err(Error::make(
                    e.what(), "E0200", Span{t.start, t.end}, raw_input_));
            }
        }

        // Final fallback: treat all unhandled input as a math expression.
        // - Input is trimmed to avoid spurious whitespace in the command node.
        // - Invariant: all code paths yield a non-null command node.
        return Result<CommandPtr>::ok(std::make_unique<MathCommand>(
            MathCommand::Type::Evaluate, trim(raw_input_), raw_input_));
    }

    // All subparser logic is intentionally delegated to the registry or
    // SystemCommandParser. This type is kept minimal to avoid tight coupling
    // with subparser implementations.
    Result<CommandPtr> parse_command(const std::string& input) {
        CommandParser parser(input);
        return parser.parse();
    }

} // namespace math_solver
