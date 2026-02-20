#include "runner.hpp"
#include "ast/command/load_command.hpp"
#include "parser/command/command_parser.hpp"
#include "ui/color.hpp"
#include "utils/string_utils.hpp"

#include <cerrno>
#include <fstream>
#include <iostream>

namespace math_solver {

    // Prompt must encode the current environment name to avoid ambiguity
    // when switching contexts. Any change here must be coordinated with
    // completion and hinting logic elsewhere to avoid desynchronization.
    std::string build_prompt(const std::string& env_name) {
        return std::string(ansi::bold) + "[" + env_name + "]" + ansi::reset +
               " > ";
    }

    // Assumes parse_command returns a valid command or throws.
    // Returns false if the command signals REPL termination.
    bool Runner::run_line(const std::string& line) {
        auto cmd = parse_command(line);
        return registry_.dispatch(*cmd);
    }

    // Main interactive REPL loop.
    // - Handles EINTR/EAGAIN from input to avoid spurious termination.
    // - Maintains history for user convenience.
    // - Exits on EOF or explicit command.
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

            rx.history_add(line);

            if (!run_line(line))
                break;
        }
    }

    // Script execution:
    // - Ignores blank lines and lines starting with '#'.
    // - Reports errors per-line but continues execution to maximize coverage.
    // - Flags.silent suppresses echoing commands; flags.dry_run disables
    // execution.
    // - Reports aggregate error count at end for diagnostics.
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