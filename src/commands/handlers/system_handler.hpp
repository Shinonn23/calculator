#pragma once

#include "ast/command/history_entry.hpp"
#include "ast/command/system_command.hpp"
#include "config/config.hpp"
#include "core/error.hpp"
#include "runtime/context/context.hpp"
#include "ui/color.hpp"

#include <iomanip>
#include <iostream>
#include <string>

namespace math_solver {
    namespace handlers {

        // Centralized help output for all supported commands.
        //
        // - Must be kept in sync with parser and command set; any additions or
        //   removals require updating this output to avoid user confusion.
        // - If the command set grows substantially, consider extracting help
        //   text to a single authoritative source to avoid duplication.
        // - Not performance critical; invoked only on explicit user request.
        inline void print_help() {
            using std::cout;

            auto section = [&](const char* title) {
                cout << "\n" << ansi::bold << title << ansi::reset << "\n";
            };

            auto cmd = [&](const char* name, const char* desc) {
                cout << "  " << ansi::bold << std::left << std::setw(32) << name
                     << ansi::reset << desc << "\n";
            };

            auto flag = [&](const char* name, const char* desc) {
                cout << "    " << ansi::dim << std::left << std::setw(30)
                     << name << ansi::reset << desc << "\n";
            };

            auto note = [&](const char* text) {
                cout << "  " << ansi::dim << text << ansi::reset << "\n";
            };

            cout << "\n"
                 << ansi::bold
                 << "╔══════════════════════════════════════════════════════╗\n"
                 << "║              CMath Solver  —  Help Menu              ║\n"
                 << "╚══════════════════════════════════════════════════════╝"
                 << ansi::reset << "\n";

            section("1. Evaluation & Equations");
            cmd("<expr>", "Evaluate an expression");
            note("e.g.  2 + 3 * (5^2)   or   sin(pi/2)");
            cmd("<lhs> = <rhs>", "Check equality (returns true/false)");
            cmd(":solve <eq>", "Solve for the unknown, auto-saves result");
            note("e.g.  :solve 2x + 3 = 7");
            cmd(":simplify <eq>", "Canonicalise equation to Ax + By = C form");
            flag("--vars x y", "Force variable ordering in output");
            flag("--isolated",
                 "Treat all identifiers as unknowns (ignore context)");
            flag("--fraction", "Display coefficients as fractions");
            cmd(":expand <expr>", "Expand to standard polynomial form");
            note("e.g.  :expand (x+1)^3");
            cmd(":factor <expr>", "Factorise a polynomial expression");

            section("2. Variable Management");
            cmd(":set <var> <expr>",
                "Bind a variable to an expression or value");
            note("e.g.  :set g 9.81   or   :set r 3/4");
            cmd(":set <var> solve <eq>", "Solve and store result in <var>");
            cmd(":set <var> expand <expr>", "Expand and store result in <var>");
            cmd(":set <var> factor <expr>",
                "Factorise and store result in <var>");
            cmd(":unset <var>", "Remove a variable from the current context");
            cmd(":rm <var>", "Alias for :unset");
            cmd(":ls", "List all variables with their current values");
            cmd(":clear", "Remove all variables from the current environment");

            section("3. Environments  (isolated variable workspaces)");
            cmd(":env", "Show the name of the active environment");
            cmd(":env list", "List all environments  (* marks active)");
            cmd(":env new <name>", "Create a new empty environment");
            cmd(":env load <name>",
                "Switch to an environment (saves current first)");
            cmd(":env save [name]",
                "Persist current variables (defaults to active env)");
            flag("--vars x y", "Save only specific variables");
            cmd(":env delete <name>",
                "Delete an environment (active env is protected)");
            cmd(":env mv <src> <dst>", "Rename an environment");
            cmd(":env cp <src> <dst>", "Duplicate an environment");
            cmd(":env mv --vars x y --to <env>",
                "Move variables to another env (removes from current)");
            cmd(":env cp --vars x y --to <env>",
                "Copy variables to another env (keeps in current)");

            section("4. History");
            cmd(":history", "Show last 20 commands");
            cmd(":history <n>", "Show last n commands");
            cmd(":history all", "Show entire history");
            cmd(":history <n> <m>", "Show commands in index range [n, m]");
            cmd(":history search <pat>", "Filter history by pattern");
            note("e.g.  :history search :set");
            cmd(":history save <file>",
                "Export history as a runnable .msl script");
            note("e.g.  :history save session.msl   or   :history save out.msl "
                 "1,3,5-8");
            flag("--errors", "Show only commands that resulted in an error");
            cmd(":history clear", "Clear all history (in-memory and on disk)");

            section("5. Redo");
            cmd(":redo", "Re-execute the last command");
            cmd(":redo <n>", "Re-execute command at index n");
            cmd(":redo <selector>",
                "Re-execute a set of commands by index/range");
            note("selector syntax:  5   or   1,3,7   or   4-6   or   1,3,5-8");

            section("6. Scripts  (.msl files)");
            cmd(":load <file.msl>", "Run all commands from a script file");
            flag("--dry-run", "Parse and echo commands without executing");
            flag("--silent", "Suppress output except errors");
            flag("--env <name>",
                 "Execute script inside a specific environment");
            note("Script format: one command per line; lines starting with # "
                 "are comments");
            note("e.g.  :load setup.msl   or   :history save today.msl  →  "
                 ":load today.msl");

            section("7. Configuration");
            cmd(":config", "List all settings and their current values");
            cmd(":config get <key>", "Show the value of a specific setting");
            cmd(":config set <key> <val>", "Update a setting");
            note("e.g.  :config set output.fraction true");
            cmd(":config path", "Show the path to the config file");
            cmd(":config reset", "Restore all settings to factory defaults");

            section("8. System");
            cmd(":help  /  :h", "Show this help menu");
            cmd(":clear  /  :cls", "Clear the terminal screen");
            cmd("exit  /  quit  /  :q", "Exit the solver");

            cout << "\n" << std::string(56, '-') << "\n";
        }

        // Handles system-level commands (exit, help, clear, ls).
        //
        // - Invariant: `cmd` must be a valid SystemCommand variant as produced
        // by the parser.
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
                // at a higher layer. Must ensure no further commands are
                // processed after this returns Success with out_should_exit =
                // true.
                out_should_exit = true;
                return HistoryStatus::Success;

            case SystemCommand::Type::Help:
                // Help output is side-effect free and does not mutate any
                // state.
                print_help();
                return HistoryStatus::Info;

            case SystemCommand::Type::Clear:
                // Emits ANSI escape codes to clear the terminal.
                // Not guaranteed to work on all terminals; no fallback
                // provided.
                std::cout << "\033[2J\033[H";
                return HistoryStatus::Info;

            case SystemCommand::Type::Ls: {
                // Variable listing is snapshot-based; assumes ctx.all() returns
                // a stable view of the current context. Output is aligned for
                // readability. If context is empty, emits a message.
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
                // Defensive: parser should not emit Unknown unless input is
                // unrecognized. Error is formatted and printed for user
                // feedback.
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
