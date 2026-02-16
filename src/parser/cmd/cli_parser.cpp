#include "cli_parser.hpp"
#include "../../common/utils.hpp"

#include <algorithm>
#include <unordered_map>

namespace math_solver {

    // ── Comment stripping ───────────────────────────────────────────────

    std::string strip_comment(const std::string& input) {
        size_t pos = input.find(" --");
        if (pos != std::string::npos) {
            size_t after = pos + 3;
            if (after >= input.size() || input[after] == ' ')
                return trim(input.substr(0, pos));
        }
        if (input.size() >= 2 && input[0] == '-' && input[1] == '-') {
            if (input.size() <= 2 || input[2] == ' ')
                return "";
        }
        return input;
    }

    // ── Lookup tables ───────────────────────────────────────────────────

    static const std::unordered_map<std::string, CommandType> COMMAND_TABLE = {
        {"set",       CommandType::Set},
        {"unset",     CommandType::Unset},
        {"rm",        CommandType::Unset},
        {"clear",     CommandType::Clear},
        {"cls",       CommandType::Clear},
        {"ls",        CommandType::Ls},
        {"help",      CommandType::Help},
        {"h",         CommandType::Help},
        {"config",    CommandType::Config},
        {"conf",      CommandType::Config},
        {"env",       CommandType::Env},
        {"solve",     CommandType::Solve},
        {"simplify",  CommandType::Simplify},
        {"expand",    CommandType::Expand},
        {"factor",    CommandType::Factor},
    };

    static const std::unordered_map<std::string, ValueNode::Kind> VALUE_KEYWORD_TABLE = {
        {"solve",     ValueNode::SolveExpr},
        {"expand",    ValueNode::ExpandExpr},
        {"factor",    ValueNode::FactorExpr},
        {"simplify",  ValueNode::SimplifyExpr},
    };

    // ── Helpers ─────────────────────────────────────────────────────────

    static bool is_flag(const std::string& token) {
        return token.size() >= 2 && token[0] == '-' && std::isalpha(token[1]);
    }

    static bool is_value_keyword(const std::string& token) {
        return VALUE_KEYWORD_TABLE.count(token) > 0;
    }

    static std::string join_tokens(const std::vector<std::string>& tokens,
                                   size_t start, size_t end) {
        std::string result;
        for (size_t i = start; i < end; ++i) {
            if (i > start) result += " ";
            result += tokens[i];
        }
        return result;
    }

    // ── Flag + value parsing ────────────────────────────────────────────

    static void parse_flags_and_value(const std::vector<std::string>& tokens,
                                      size_t start, CommandAST& ast) {
        size_t i = start;

        // 1. Consume flags (must come before value keyword)
        while (i < tokens.size()) {
            if (!is_flag(tokens[i])) break;

            const std::string& flag = tokens[i];
            if (flag == "-isolated" || flag == "--isolated") {
                ast.flags.push_back(FlagType::Isolated);
                ++i;
            } else if (flag == "-fraction" || flag == "--fraction") {
                ast.flags.push_back(FlagType::Fraction);
                ++i;
            } else if (flag == "-approx" || flag == "--approx") {
                ast.flags.push_back(FlagType::Approx);
                ++i;
            } else if (flag == "-vars" || flag == "--vars") {
                ast.flags.push_back(FlagType::Vars);
                ++i;
                while (i < tokens.size() && !is_flag(tokens[i]) &&
                       !is_value_keyword(tokens[i])) {
                    ast.flag_vars.push_back(tokens[i]);
                    ++i;
                }
            } else {
                break; // unknown flag — stop
            }
        }

        if (i >= tokens.size()) return;

        // 2. Value keyword or raw expression
        auto it = VALUE_KEYWORD_TABLE.find(tokens[i]);
        if (it != VALUE_KEYWORD_TABLE.end()) {
            ast.value = ValueNode{it->second,
                                  join_tokens(tokens, i + 1, tokens.size())};
        } else {
            ast.value = ValueNode{ValueNode::RawExpr,
                                  join_tokens(tokens, i, tokens.size())};
        }
    }

    // ── Resolve command type from string ─────────────────────────────────

    static CommandType resolve_command(const std::string& name) {
        auto it = COMMAND_TABLE.find(name);
        return (it != COMMAND_TABLE.end()) ? it->second : CommandType::Unknown;
    }

    // ── Main entry ──────────────────────────────────────────────────────

    CommandAST parse_cli(const std::string& raw_input) {
        std::string input = strip_comment(raw_input);
        input = trim(input);

        // Empty input
        if (input.empty()) {
            CommandAST ast;
            ast.type = CommandType::MathExpr;
            return ast;
        }

        // exit / quit / q
        if (input == "exit" || input == "quit" || input == "q") {
            CommandAST ast;
            ast.type = CommandType::Exit;
            return ast;
        }

        // Must start with ':' to be a CLI command
        if (input[0] != ':') {
            // Bare math expression
            CommandAST ast;
            ast.type  = CommandType::MathExpr;
            ast.value = ValueNode{ValueNode::RawExpr, input};
            return ast;
        }

        // Strip ':'
        input = trim(input.substr(1));
        if (input.empty()) {
            CommandAST ast;
            ast.type = CommandType::Unknown;
            ast.raw_command = ":";
            return ast;
        }

        std::vector<std::string> tokens = split(input);
        CommandAST ast;
        ast.raw_command = tokens[0];
        ast.type        = resolve_command(tokens[0]);

        switch (ast.type) {
        // ── Simple commands ─────────────────────────────────────────
        case CommandType::Exit:
        case CommandType::Help:
        case CommandType::Clear:
            return ast;

        case CommandType::Ls:
            return ast;

        case CommandType::Unset:
            if (tokens.size() > 1) ast.args.push_back(tokens[1]);
            return ast;

        // ── Config / Env (have subcommands) ─────────────────────────
        case CommandType::Config:
        case CommandType::Env:
            if (tokens.size() > 1) {
                ast.subcommand = tokens[1];
                for (size_t i = 2; i < tokens.size(); ++i)
                    ast.args.push_back(tokens[i]);
            }
            return ast;

        // ── Set: :set <var> [flags...] [value-op] <expr> ────────────
        case CommandType::Set:
            if (tokens.size() < 2) return ast;
            ast.args.push_back(tokens[1]); // var name
            parse_flags_and_value(tokens, 2, ast);
            return ast;

        // ── Top-level math ops: :solve / :expand / :factor / :simplify
        case CommandType::Solve:
        case CommandType::Expand:
        case CommandType::Factor:
        case CommandType::Simplify: {
            auto it = VALUE_KEYWORD_TABLE.find(tokens[0]);
            if (it != VALUE_KEYWORD_TABLE.end()) {
                // Consume flags, then payload
                parse_flags_and_value(tokens, 1, ast);
                // If parse_flags_and_value didn't set value (no payload after
                // flags), set it with empty payload and the correct kind
                if (!ast.value) {
                    ast.value = ValueNode{it->second, ""};
                } else if (ast.value->kind == ValueNode::RawExpr) {
                    // Flags consumed, remaining was raw — promote to correct kind
                    ast.value = ValueNode{it->second, ast.value->payload};
                }
            }
            return ast;
        }

        // ── Unknown ─────────────────────────────────────────────────
        case CommandType::Unknown:
        default:
            for (size_t i = 1; i < tokens.size(); ++i)
                ast.args.push_back(tokens[i]);
            return ast;
        }
    }

} // namespace math_solver
