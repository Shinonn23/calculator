#pragma once

#include "ast/command/history_entry.hpp"
#include "ast/command/system_command.hpp"
#include "config/config.hpp"
#include "core/error.hpp"
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
                << ansi::bold
                << "╔══════════════════════════════════════════════════════╗"
                << ansi::reset << "\n"
                << ansi::bold
                << "║                CMath Solver - Help Menu              ║"
                << ansi::reset << "\n"
                << ansi::bold
                << "╚══════════════════════════════════════════════════════╝"
                << ansi::reset << "\n\n"

                << ansi::bold << "1. [ Evaluation & Equations ]" << ansi::reset
                << "\n"
                << "   <expr>                      Evaluate expression (e.g., "
                   "2 + 3 * (5^2))\n"
                << "   <lhs> = <rhs>               Check if the equation is "
                   "true or false\n"
                << "   :solve <eq>                 Solve for a single unknown "
                   "(auto-saves result)\n"
                << "   :simplify <eq>              Simplify equation to "
                   "canonical form\n"
                << "      -vars x y                (Flag) Define variable "
                   "ordering\n"
                << "      -isolated                (Flag) Do not substitute "
                   "context variables\n"
                << "      -fraction                (Flag) Display results as "
                   "fractions\n\n"

                << ansi::bold << "2. [ Variable Management ]" << ansi::reset
                << "\n"
                << "   :set <var> <expr>           Create or update a variable "
                   "(e.g., :set x 10)\n"
                << "   :set <var> <cmd> <input>    Execute a command and store "
                   "result in <var>\n"
                << "                               (e.g., :set x solve y+2=5)\n"
                << "   :unset <var>                Remove a specific variable\n"
                << "   :ls                         List all variables and "
                   "their current values\n"
                << "   :clear                      Clear all variables in the "
                   "current environment\n\n"

                << ansi::bold << "3. [ Polynomial Operations ]" << ansi::reset
                << "\n"
                << "   :expand <expr>              Expand polynomials (e.g., "
                   "(x+1)^2 -> x^2+2x+1)\n"
                << "   :factor <expr>              Factorize a polynomial "
                   "expression\n\n"

                << ansi::bold << "4. [ Environments (Context) ]" << ansi::reset
                << "\n"
                << "   :env                        Show the name of the active "
                   "environment\n"
                << "   :env list                   List all available "
                   "environments\n"
                << "   :env new <name>             Create a new environment "
                   "(workspace)\n"
                << "   :env load <name>            Switch to a specific "
                   "environment\n"
                << "   :env delete <name>          Delete an environment\n"
                << "   :env cp <src> <dst>         Copy all variables from src "
                   "to dst environment\n"
                << "   :env mv <src> <dst>         Rename an environment\n"
                << "   :env save [name]            Persist variables to "
                   "storage\n\n"

                << ansi::bold << "5. [ System & History ]" << ansi::reset
                << "\n"
                << "   :history [n]                Show command history (n for "
                   "last n entries)\n"
                << "   :history search <pat>       Search history for a "
                   "specific pattern\n"
                << "   :history save <file>        Export history as a script "
                   "file (.msl)\n"
                << "   :history clear              Clear all command history\n"
                << "   :redo [n]                   Re-execute last command (or "
                   "nth command)\n"
                << "   :load <filepath>            Execute commands from an "
                   "external script\n"
                << "      --dry-run                (Flag) Parse only, do not "
                   "execute commands\n"
                << "      --silent                 (Flag) Execute without "
                   "displaying output\n\n"

                << ansi::bold << "6. [ Configuration ]" << ansi::reset << "\n"
                << "   :config list                Show all settings "
                   "(Precision, Fraction Mode, etc.)\n"
                << "   :config set <key> <val>     Update a specific "
                   "configuration value\n"
                << "   :config reset               Restore all settings to "
                   "factory defaults\n\n"

                << "   :help                       Show this help menu\n"
                << "   exit / quit / :q            Exit the solver\n"
                << std::string(56, '-') << "\n";
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
                                           Context& ctx, Config& /*config*/,
                                           bool&    out_should_exit) {
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
            case SystemCommand::Type::Unknown: {
                std::string         input   = cmd.raw_command();

                std::string         bad_cmd = input.substr(0, input.find(' '));

                UnknownCommandError e       = UnknownCommandError(
                    bad_cmd, find_token_span(input, bad_cmd), input);

                std::cout << e.format() << "\n";
                return HistoryStatus::Error;
            }
            }
            return HistoryStatus::Unknown;
        }
    } // namespace handlers
} // namespace math_solver