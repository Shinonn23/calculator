#include "runner.hpp"
#include "ast/command/load_command.hpp"
#include "diagnostics/sink.hpp"
#include "parser/command/command_parser.hpp"
#include "runtime/runtime.hpp"
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
            registry_.sink().push(parse_result.error());
            registry_.sink().flush(std::cout);
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
        last_script_had_errors_ = false;
        std::ifstream file(filepath);
        if (!file.is_open()) {
            std::cout << ansi::red << "  Error: " << ansi::reset
                      << "cannot open '" << filepath << "'\n";
            last_script_had_errors_ = true;
            return;
        }

        RuntimeSnapshot snap{registry_.ctx(), registry_.config(),
                             registry_.current_env()};

        std::string     old_env;
        bool            should_switch_back = false;
        if (!flags.env.empty()) {
            if (!registry_.config().env_exists(flags.env)) {
                std::cout << ansi::red << "  Error: " << ansi::reset
                          << "environment '" << flags.env
                          << "' does not exist\n";
                last_script_had_errors_ = true;
                return;
            }
            old_env            = registry_.current_env();
            should_switch_back = true;
            // Temporarily use the fallback sink for run_line
            run_line(":env load " + flags.env);
        }

        DiagnosticSink sink(DiagnosticSink::Options(50, true, true));

        std::string    line;
        size_t         line_no = 0, total = 0;
        bool           should_rollback    = false;
        int            unaccounted_errors = 0;

        while (std::getline(file, line)) {
            ++line_no;
            auto trimmed = trim(line);
            if (trimmed.empty() || trimmed[0] == '#')
                continue;
            ++total;

            if (!flags.silent)
                std::cout << ansi::dim << "  [" << line_no << "] " << trimmed
                          << ansi::reset << "\n";

            if (flags.dry_run)
                continue;

            auto loc    = SourceLocation::from_file(filepath, line_no);
            auto result = parse_command(trimmed);

            if (!result) {
                sink.push(result.error().with_location(loc));
                should_rollback = true;
                if (flags.strict)
                    break;
                continue;
            }

            auto& cmd = *result;
            cmd->set_source(filepath, line_no);

            size_t errors_before = sink.error_count();
            registry_.dispatch(*cmd, sink);
            if (!flags.silent) {
                sink.flush_outputs(std::cout);
            } else {
                sink.clear_outputs();
            }

            if (registry_.last_command_status() == HistoryStatus::Error) {
                if (sink.error_count() == errors_before) {
                    // Handler failed but didn't push to sink; it emitted
                    // directly.
                    unaccounted_errors++;
                }
                should_rollback = true;
                if (flags.strict)
                    break;
            } else if (sink.has_errors()) {
                should_rollback = true;
                if (flags.strict)
                    break;
            }
        }

        if (should_rollback) {
            last_script_had_errors_ = true;
        }

        if (should_rollback && !flags.no_rollback) {
            registry_.ctx()             = snap.ctx;
            registry_.config()          = snap.config;
            registry_.current_env_mut() = snap.current_env;

            registry_.config().save();

            sink.flush_summary(filepath, total);
            std::cout << ansi::yellow << ansi::dim
                      << "  Rolled back — env and config unchanged\n"
                      << ansi::reset;
            return;
        }

        if (should_switch_back) {
            run_line(":env load " + old_env);
        }

        int total_errors = sink.error_count() + unaccounted_errors;

        if (unaccounted_errors > 0 || (!flags.silent && total_errors > 0)) {
            sink.flush(); // Flush any warnings/errors inside sink
            std::cout << "  Loaded '" << filepath << "' (" << total
                      << (total == 1 ? " line)" : " lines)");
            if (total_errors > 0)
                std::cout << ", " << ansi::red << total_errors << " error(s)"
                          << ansi::reset;
            std::cout << "\n";
        } else if (!flags.silent) {
            sink.flush_summary(filepath, total);
        } else if (total_errors == 0) {
            // Flush warnings without the summary if it was silent and
            // successful
            sink.flush();
        }
    }

    // Thin wrapper for registry_ dispatch; exists for interface uniformity.
    bool Runner::dispatch_cmd(CommandPtr& cmd) {
        return registry_.dispatch(*cmd);
    }

} // namespace math_solver
