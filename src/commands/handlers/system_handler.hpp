#pragma once

#include "ast/command/system_command.hpp"
#include "config/config.hpp"
#include "runtime/context/context.hpp"
#include "ui/color.hpp"

#include <iostream>
#include <string>

namespace math_solver {
    namespace handlers {
        // Prints the help message for all supported commands.
        // This is invoked on demand and not cached, as the output is small and
        // infrequently requested. Any changes to the command set must be kept
        // in sync here; consider extracting to a single source if this grows.
        inline void print_help() {
            using std::cout;
            cout << "\n"
                 << ansi::bold << "Math Solver " << ansi::reset
                 << "- Commands\n"
                 << std::string(40, '-') << "\n\n"
                 << ansi::bold << "Evaluation\n"
                 << ansi::reset
                 << "  <expr>                     Evaluate expression (e.g. 2 "
                    "+ 3 * 4)\n"
                 << "  <lhs> = <rhs>              Check equality\n\n"
                 << ansi::bold << "Variables\n"
                 << ansi::reset << "  :set <var> <expr>          Set variable\n"
                 << "  :set <var> solve <eq>      Solve and store\n"
                 << "  :set <var> expand <expr>   Expand and store\n"
                 << "  :set <var> factor <expr>   Factor and store\n"
                 << "  :unset <var>               Remove variable\n"
                 << "  :ls                        Show all variables\n\n"
                 << ansi::bold << "Equations\n"
                 << ansi::reset
                 << "  :solve <lhs> = <rhs>       Solve equation (auto-saves "
                    "result)\n"
                 << "  :simplify <lhs> = <rhs>    Simplify to canonical form\n"
                 << "    -vars x y                  Variable order\n"
                 << "    -isolated                  Don't substitute context "
                    "vars\n"
                 << "    -fraction                  Display as fractions\n\n"
                 << ansi::bold << "Polynomial\n"
                 << ansi::reset
                 << "  :expand <expr>             Expand to canonical form\n"
                 << "  :factor <expr>             Factor a polynomial\n\n"
                 << ansi::bold << "Config\n"
                 << ansi::reset
                 << "  :config list               Show all settings\n"
                 << "  :config get <key>          Show setting value\n"
                 << "  :config set <key> <val>    Update setting\n"
                 << "  :config path               Show config file path\n"
                 << "  :config reset              Reset to defaults\n\n"
                 << ansi::bold << "Environments\n"
                 << ansi::reset
                 << "  :env                       Show current environment\n"
                 << "  :env list                  List all environments\n"
                 << "  :env load <name>           Switch to environment\n"
                 << "  :env save [name] [vars]    Save variables to env\n"
                 << "  :env new <name>            Create new environment\n"
                 << "  :env delete <name>         Delete environment\n\n"
                 << ansi::bold << "Other\n"
                 << ansi::reset
                 << "  :help                      Show this help\n"
                 << "  exit / quit                Exit\n\n";
        }

        // Handles system-level commands (exit, help, clear, ls).
        // Returns true if the REPL should terminate.
        //
        // Invariant: `cmd` must be a valid SystemCommand.
        // Context (`ctx`) is mutated only for commands that require it.
        // This function is performance-insensitive, as system commands are
        // rare. Any changes to the set of handled commands must be reflected in
        // both the parser and this handler to avoid silent no-ops.
        inline bool handle_system(const SystemCommand& cmd,
                                  Context&             ctx,
                                  Config& /*config*/) {
            switch (cmd.type()) {
            case SystemCommand::Type::Exit:
                // Signal REPL termination. No cleanup is performed here;
                // resource management is handled at a higher layer.
                return true;

            case SystemCommand::Type::Help:
                print_help();
                return false;

            case SystemCommand::Type::Clear:
                // Emits ANSI escape codes to clear the terminal.
                // This is a best-effort operation; not all terminals may
                // support it.
                std::cout << "\033[2J\033[H";
                return false;

            case SystemCommand::Type::Ls: {
                // List all variables in the current context.
                // Output is aligned for readability. If the context is empty,
                // a message is printed instead. Assumes ctx.all() returns a
                // stable snapshot of the current variable map.
                if (ctx.empty()) {
                    std::cout << "  No variables defined\n";
                    return false;
                }
                size_t max_len = 0;
                for (const auto& [name, _] : ctx.all())
                    max_len = std::max(max_len, name.size());
                for (const auto& [name, expr] : ctx.all()) {
                    std::cout << "  " << name;
                    for (size_t i = name.size(); i < max_len; ++i)
                        std::cout << ' ';
                    std::cout << "  =  " << expr->to_string() << "\n";
                }
                return false;
            }
            }
            // Defensive: unknown SystemCommand type. Should be unreachable if
            // all enum variants are handled above. If new variants are added,
            // this will default to no-op.
            return false;
        }

    } // namespace handlers
} // namespace math_solver