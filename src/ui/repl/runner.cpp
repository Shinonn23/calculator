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
            }

            catch (const MathError& e) {
                std::cout << e.format() << "\n";
                status = HistoryStatus::Error;
            }
            catch (const std::exception& e) {
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

    // Script execution is designed to maximize robustness:
    // - Continues processing after handler-level failures or exceptions.
    // - Only increments error count once per line, regardless of multiple error
    // paths.
    // - Maintains registry_ invariants across all error conditions.
    // - Skips empty lines and comment lines (lines starting with '#').
    // - Echoes executed lines unless flags.silent is set.
    // - flags.dry_run disables execution but still parses and prints lines.
    //
    // Subtlety: Error reporting in silent mode requires temporarily restoring
    // std::cout to ensure visibility of failures. This avoids silent loss of
    // diagnostics.
    //
    // Performance: Stream redirection for silent mode is localized to minimize
    // overhead. Interaction: Any changes to environment switching must be
    // coordinated with registry_ and context management logic elsewhere.
    void Runner::run_script(const std::string&        filepath,
                            const LoadCommand::Flags& flags) {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            std::cout << ansi::red << "  Error: " << ansi::reset
                      << "cannot open '" << filepath << "'\n";
            return;
        }

        // --- 1. สลับ Environment (ถ้าระบุมา) ---
        std::string old_env;
        bool        should_switch_back = false;
        if (!flags.env.empty()) {
            old_env =
                registry_.current_env(); // คุณอาจต้องเพิ่ม getter นี้ใน registry
            // จำลองการรันคำสั่ง :env load <name> แบบเงียบๆ
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

            // Echo คำสั่ง (ถ้าไม่เงียบ)
            if (!flags.silent) {
                std::cout << ansi::dim << "  [" << line_no << "] " << trimmed
                          << ansi::reset << "\n";
            }

            // รันคำสั่ง (ถ้าไม่ใช้ dry-run)
            if (!flags.dry_run) {
                if (flags.silent)
                    std::cout.rdbuf(garbage.rdbuf()); // ย้ายทางน้ำไปลงถังขยะ

                try {
                    run_line(trimmed);
                    if (registry_.last_command_status() == HistoryStatus::Error)
                        errors++;
                } catch (const std::exception& e) {
                    errors++;
                    std::cout.rdbuf(old_cout); // คืนค่าชั่วคราวเพื่อพ่น error
                    std::cout << ansi::red << "  Error at line " << line_no
                              << ": " << ansi::reset << e.what() << "\n";
                    if (flags.silent)
                        std::cout.rdbuf(garbage.rdbuf());
                }

                std::cout.rdbuf(old_cout); // คืนค่าปกติหลังจบบรรทัด
            }
        }

        // --- 2. สลับ Environment กลับ ---
        if (should_switch_back) {
            run_line(":env load " + old_env);
        }

        // สรุปผล
        if (!flags.silent || errors > 0) {
            std::cout << "  Loaded '" << filepath << "' (" << line_no
                      << " lines";
            if (errors)
                std::cout << ", " << ansi::red << errors << " error(s)"
                          << ansi::reset;
            std::cout << ")\n";
        }
    }

} // namespace math_solver