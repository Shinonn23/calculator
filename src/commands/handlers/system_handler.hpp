#pragma once

#include "ast/command/history_entry.hpp"
#include "ast/command/system_command.hpp"
#include "config/config.hpp"
#include "runtime/context/context.hpp"
#include "ui/color.hpp"

#include <iostream>
#include <string>

namespace math_solver {
    namespace handlers {

        // Prints the help message for all supported commands.
        //
        // - Not performance critical; invoked only on explicit user request.
        // - Output must be kept in sync with the parser and command set.
        // - Any additions to the command set require updating this output.
        // - If the command set grows, consider extracting help text to a single
        // source.
        inline void print_help() {
            using std::cout;
            cout
                << "\n"
                << ansi::bold << "CMath Solver " << ansi::reset
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
                << "  :env delete <name>         Delete environment\n"
                << "  :env mv <src> <dstenv>     Rename environment <src> to "
                   "<dstenv>\n"
                << "  :env cp <src> <dstenv>     Duplicate environment <src> "
                   "as <dstenv>\n"
                << "  :env mv --vars x y --to <dstvars>  Move variables x, y "
                   "to <dstvars> and remove from current\n"
                << "  :env cp --vars x y --to <dstvars>  Copy variables x, y "
                   "to <dstvars> (keep in current)\n\n"
                << ansi::bold << "File Loading\n"
                << ansi::reset
                << "  :load <filepath>           Load commands from file\n"
                << "    --dry-run                  Parse only, do not execute\n"
                << "    --silent                   Suppress output\n"
                << "    --env <name>               Load into environment "
                   "<name>\n"
                << "  # Comments in load files start with '#' and continue to "
                   "end of line\n\n"
                << ansi::bold << "Other\n"
                << ansi::reset
                << "  :help                      Show this help\n"
                << "  exit / quit / q            Exit\n\n";
        }

        // Handles system-level commands (exit, help, clear, ls).
        //
        // - Invariant: `cmd` must be a valid SystemCommand variant.
        // - Only `ctx` is mutated, and only for commands that require it.
        // - Not performance sensitive; system commands are rare in normal
        // usage.
        // - All handled command variants must be kept in sync with the parser.
        // - If new SystemCommand variants are added, this function must be
        // updated
        //   to avoid silent no-ops.
        // - Returns HistoryStatus to indicate result for REPL history tracking.
        // - Sets `out_should_exit` to signal REPL termination (for Exit).
        inline HistoryStatus handle_system(const SystemCommand& cmd,
                                           Context&             ctx,
                                           Config& /*config*/,
                                           bool& out_should_exit) {
            out_should_exit = false;

            switch (cmd.type()) {
            case SystemCommand::Type::Exit:
                // Signals REPL termination. No resource cleanup here; handled
                // at a higher layer.
                out_should_exit = true;
                return HistoryStatus::Success;

            case SystemCommand::Type::Help:
                print_help();
                return HistoryStatus::Info;

            case SystemCommand::Type::Clear:
                // Emits ANSI escape codes to clear the terminal.
                // Not guaranteed to work on all terminals.
                std::cout << "\033[2J\033[H";
                return HistoryStatus::Info;

            case SystemCommand::Type::Ls: {
                // Lists all variables in the current context.
                // Output is aligned for readability.
                // If context is empty, prints a message.
                // Assumes ctx.all() returns a stable snapshot.
                if (ctx.empty()) {
                    std::cout << "  No variables defined\n";
                    return HistoryStatus::Info;
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
                return HistoryStatus::Success;
            }
            }
            // Defensive: If a new SystemCommand variant is added and not
            // handled above, this will default to HistoryStatus::Unknown.
            // Should be unreachable.
            return HistoryStatus::Unknown;
        }

    } // namespace handlers
} // namespace math_solver