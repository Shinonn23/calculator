#ifndef DISPATCHER_H
#define DISPATCHER_H

#include "cli_parser.hpp"
#include "command_ast.hpp"
#include "common/command.hpp"

#include <iostream>
#include <string>

namespace math_solver {

    // Dispatch a parsed CLI command.
    // Returns false if the REPL should exit.
    inline bool dispatch_command(const CommandAST& ast, Context& g_ctx,
                                 Config& g_config, std::string& g_current_env) {
        switch (ast.type) {

        case CommandType::Exit:
            return false;

        case CommandType::Help:
            print_help();
            return true;

        case CommandType::Clear:
            cmd_clear(g_ctx);
            return true;

        case CommandType::Ls:
            cmd_vars(g_ctx, g_config);
            return true;

        case CommandType::Unset: {
            std::string var = ast.args.empty() ? "" : ast.args[0];
            cmd_unset(var, g_ctx);
            return true;
        }

        case CommandType::Config: {
            std::string args;
            if (!ast.subcommand.empty()) {
                args = ast.subcommand;
                for (const auto& a : ast.args) args += " " + a;
            }
            cmd_config(args, g_config);
            return true;
        }

        case CommandType::Env: {
            std::string args;
            if (!ast.subcommand.empty()) {
                args = ast.subcommand;
                for (const auto& a : ast.args) args += " " + a;
            }
            cmd_env(args, g_current_env, g_config, g_ctx);
            return true;
        }

        case CommandType::Set:
            cmd_set(ast, g_ctx, g_config);
            return true;

        case CommandType::Solve:
            if (ast.value && !ast.value->payload.empty()) {
                cmd_solve(ast.value->payload, g_ctx, g_config);
            } else {
                std::cout << "  Usage: solve <lhs> = <rhs>\n";
            }
            return true;

        case CommandType::Expand:
            if (ast.value && !ast.value->payload.empty()) {
                cmd_expand(ast.value->payload);
            } else {
                std::cout << "  Usage: expand <expression>\n";
            }
            return true;

        case CommandType::Factor:
            if (ast.value && !ast.value->payload.empty()) {
                cmd_factor(ast.value->payload);
            } else {
                std::cout << "  Usage: factor <polynomial>\n";
            }
            return true;

        case CommandType::Simplify:
            if (ast.value && !ast.value->payload.empty()) {
                cmd_simplify(ast.value->payload, ast, g_config, g_ctx);
            } else {
                std::cout << "  Usage: simplify <lhs> = <rhs>\n";
            }
            return true;

        case CommandType::MathExpr:
            if (ast.value && !ast.value->payload.empty()) {
                cmd_evaluate(ast.value->payload, g_ctx, g_config);
            }
            return true;

        case CommandType::Unknown:
        default:
            std::cout << ansi::red << "  Error: " << ansi::reset
                      << "unknown command ':" << ast.raw_command << "'\n";
            maybe_suggest_command(":" + ast.raw_command, ALL_COMMANDS);
            return true;
        }
    }

} // namespace math_solver

#endif
