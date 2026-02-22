#include "runner.hpp"
#include "ast/command/load_command.hpp"
#include "core/error.hpp"
#include "parser/command/command_parser.hpp"
#include "ui/color.hpp"
#include "utils/string_utils.hpp"

#include <cerrno>
#include <fstream>
#include <iostream>

namespace math_solver {

    // Prompt encodes the current environment name.
    // Invariant: env_name must always match the active environment.
    // Any change here must be coordinated with completion/hinting logic to
    // avoid desync.
    std::string build_prompt(const std::string& env_name) {
        return std::string(ansi::bold) + "[" + env_name + "]" + ansi::reset +
               " > ";
    }

    // Assumes parse_command returns a valid command or throws on parse failure.
    // Returns false if the command signals REPL termination.
    // Invariant: registry_ must be in a valid state before and after dispatch.
    bool Runner::run_line(const std::string& line) {
        auto parse_result = parse_command(line);
        if (!parse_result) {
            std::cout << parse_result.error().format();
            return true;
        }
        auto cmd    = std::move(*parse_result);

        bool status = registry_.dispatch(*cmd);
        registry_.sink().flush(std::cout);
        return status;
    }

    // Main REPL loop.
    //
    // - Handles EINTR/EAGAIN to avoid spurious termination on signal
    // interruptions.
    // - Maintains prompt/env_name invariants; prompt must remain in sync with
    // environment.
    // - All exceptions are caught and reported; REPL remains live unless
    // explicitly terminated.
    // - History is updated with actual command status, not just parse success.
    // - Any change to prompt construction must be reflected in
    // completion/hinting logic.
    void Runner::run_interactive(replxx::Replxx& rx, std::string& current_env) {

        while (true) {
            const char* cinput;
            do {
                cinput = rx.input(build_prompt(current_env));
            } while (cinput == nullptr && errno == EAGAIN);

            if (cinput == nullptr)
                break;

            std::string line = trim(cinput);
            if (line.empty())
                continue;

            HistoryStatus status      = HistoryStatus::Unknown;
            bool          should_exit = false;

            try {
                if (!run_line(line)) {
                    should_exit = true;
                }
                status = registry_.last_command_status();
            }

            catch (const MathException& e) {
                std::cout << e.error().format() << "\n";
                status = HistoryStatus::Error;
            } catch (const std::exception& e) {
                std::cout << ansi::red << "  Error: " << ansi::reset << e.what()
                          << "\n";
                status = HistoryStatus::Error;
            }

            registry_.push_history(line, status);

            if (should_exit) {
                break;
            }
        }
    }

    // Script execution.
    //
    // - Continues after handler-level failures or exceptions; error count is
    // incremented once per line.
    // - Skips empty lines and comment lines.
    // - Echoes executed lines unless flags.silent is set.
    // - flags.dry_run disables execution but still parses and prints lines.
    // - In silent mode, error reporting temporarily restores std::cout to
    // ensure diagnostics are visible.
    // - Environment switching is emulated by running ":env load <name>" and
    // must be coordinated with registry_.
    // - Invariant: registry_ must maintain correctness across all error
    // conditions.
    // - Performance: Stream redirection for silent mode is localized to
    // minimize overhead.
    // - Any changes to environment switching must be coordinated with registry_
    // and context management logic.
    void Runner::run_script(const std::string&        filepath,
                            const LoadCommand::Flags& flags) {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            std::cout << ansi::red << "  Error: " << ansi::reset
                      << "cannot open '" << filepath << "'\n";
            return;
        }

        std::string old_env;
        bool        should_switch_back = false;
        if (!flags.env.empty()) {
            old_env = registry_.current_env();
            run_line(":env load " + flags.env);
            should_switch_back = true;
        }

        std::string       line;
        int               line_no = 0, errors = 0;
        std::stringstream garbage;
        std::streambuf*   old_cout = std::cout.rdbuf();

        while (std::getline(file, line)) {
            ++line_no;
            auto trimmed = trim(line);
            if (trimmed.empty() || trimmed[0] == '#')
                continue;

            if (!flags.silent)
                std::cout << ansi::dim << "  [" << line_no << "] " << trimmed
                          << ansi::reset << "\n";

            if (!flags.dry_run) {
                if (flags.silent)
                    std::cout.rdbuf(garbage.rdbuf());

                auto parse_result = parse_command(trimmed);
                if (!parse_result) {
                    errors++;
                    std::cout.rdbuf(old_cout);
                    std::cout << parse_result.error().format();
                    if (flags.silent)
                        std::cout.rdbuf(garbage.rdbuf());
                    continue;
                }
                auto cmd = std::move(*parse_result);

                cmd->set_source(filepath, line_no);

                registry_.dispatch(*cmd);

                if (registry_.last_command_status() == HistoryStatus::Error)
                    errors++;

                std::cout.rdbuf(old_cout);
                size_t flushed_errs =
                    registry_.sink().flush(flags.silent ? garbage : std::cout);
                if (registry_.last_command_status() != HistoryStatus::Error &&
                    flushed_errs > 0) {
                    errors += flushed_errs;
                }
            }
        }

        if (should_switch_back) {
            run_line(":env load " + old_env);
        }

        // Always print summary if errors occurred, even in silent mode.
        if (!flags.silent || errors > 0) {
            std::cout << "  Loaded '" << filepath << "' (" << line_no
                      << " lines";
            if (errors)
                std::cout << ", " << ansi::red << errors << " error(s)"
                          << ansi::reset;
            std::cout << ")\n";
        }
    }

    // Thin wrapper for registry_ dispatch; exists for interface uniformity.
    bool Runner::dispatch_cmd(CommandPtr& cmd) {
        return registry_.dispatch(*cmd);
    }

} // namespace math_solver
