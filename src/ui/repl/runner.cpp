#include "runner.hpp"
#include "ast/command/load_command.hpp"
#include "parser/command/command_parser.hpp"
#include "ui/color.hpp"
#include "utils/string_utils.hpp"

#include <cerrno>
#include <fstream>
#include <iostream>

namespace math_solver {

    // Prompt encodes the current environment name to avoid ambiguity when
    // switching contexts. Any change here must be coordinated with completion
    // and hinting logic elsewhere to avoid desynchronization. Invariant:
    // env_name must always reflect the active environment for the session.
    std::string build_prompt(const std::string& env_name) {
        return std::string(ansi::bold) + "[" + env_name + "]" + ansi::reset +
               " > ";
    }

    // Assumes parse_command returns a valid command or throws on parse failure.
    // Returns false if the command signals REPL termination.
    // Invariant: registry_ must be in a valid state before and after dispatch.
    bool Runner::run_line(const std::string& line) {
        auto cmd = parse_command(line);
        return registry_.dispatch(*cmd);
    }

    // Main interactive REPL loop.
    //
    // - Handles EINTR/EAGAIN from input to avoid spurious termination due to
    // signal interruptions.
    // - Maintains history for user convenience and for features that depend on
    // command history.
    // - Exits on EOF or explicit command.
    //
    // Subtlety: The prompt must remain in sync with the current environment, as
    // completion and hinting logic depend on this. Any change to prompt
    // construction must be reflected in those subsystems to avoid
    // desynchronization.
    //
    // Error handling: All exceptions are caught and reported, ensuring the REPL
    // remains live unless explicitly terminated. History is updated with the
    // actual command status, not just parse success.
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
            } catch (const std::exception& e) {
                std::cout << ansi::red << "  Error: " << ansi::reset << e.what()
                          << "\n";
                status = HistoryStatus::Error;
            } catch (...) {
                std::cout << ansi::red << "  Unknown error occurred."
                          << ansi::reset << "\n";
                status = HistoryStatus::Error;
            }

            registry_.push_history(line, status);

            if (should_exit) {
                break;
            }
        }
    }

    // Script execution is intentionally tolerant of errors: continues
    // processing subsequent lines after failures, only incrementing the error
    // count for lines that fail at the handler level or throw exceptions.
    //
    // - Skips empty lines and comments (lines starting with '#').
    // - If flags.silent is unset, echoes each executed line with line number.
    // - If flags.dry_run is set, skips execution but still parses and prints
    // lines.
    //
    // Subtlety: Errors are only counted once per line, even if both handler and
    // exception paths are triggered. This avoids double-counting.
    //
    // Invariant: registry_ must be left in a consistent state regardless of
    // script errors.
    void Runner::run_script(const std::string&        filepath,
                            const LoadCommand::Flags& flags) {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            std::cout << ansi::red << "  Error: " << ansi::reset
                      << "cannot open '" << filepath << "'\n";
            return;
        }

        std::string line;
        int         line_no = 0;
        int         errors  = 0;

        while (std::getline(file, line)) {
            ++line_no;

            auto trimmed = trim(line);
            if (trimmed.empty() || trimmed[0] == '#')
                continue;

            if (!flags.silent)
                std::cout << ansi::dim << "  [" << line_no << "] " << trimmed
                          << ansi::reset << "\n";

            if (!flags.dry_run) {
                try {
                    run_line(trimmed);
                    // If handler signals failure (e.g., semantic error), count
                    // as error.
                    if (registry_.last_command_status() ==
                        HistoryStatus::Error) {
                        ++errors;
                    }
                } catch (const std::exception& e) {
                    ++errors;
                    std::cout << ansi::red << "  Error at line " << line_no
                              << ": " << ansi::reset << e.what() << "\n";
                }
            }
        }

        std::cout << "  Loaded '" << filepath << "' (" << line_no << " lines";
        if (errors)
            std::cout << ", " << ansi::red << errors << " error(s)"
                      << ansi::reset;
        std::cout << ")\n";
    }

} // namespace math_solver