#include "commands/registry.hpp"

#include "ast/command/var_command.hpp"
#include "commands/handlers/config_handler.hpp"
#include "commands/handlers/env_handler.hpp"
#include "commands/handlers/history_handler.hpp"
#include "commands/handlers/load_handler.hpp"
#include "commands/handlers/math_handler.hpp"
#include "commands/handlers/redo_handler.hpp"
#include "commands/handlers/system_handler.hpp"
#include "commands/handlers/var_handler.hpp"
#include "parser/command/command_parser.hpp"

#include <chrono>
#include <iomanip>
#include <sstream>

namespace math_solver {

    // Generates a timestamp string with millisecond precision.
    // Used for history entries; relies on system clock.
    static std::string get_current_timestamp() {
        auto now        = std::chrono::system_clock::now();
        auto time_t_now = std::chrono::system_clock::to_time_t(now);
        auto ms         = std::chrono::duration_cast<std::chrono::milliseconds>(
                      now.time_since_epoch()) %
                  1000;

        std::stringstream ss;
        ss << std::put_time(std::localtime(&time_t_now), "%Y-%m-%d %H:%M:%S")
           << '.' << std::setfill('0') << std::setw(3) << ms.count();
        return ss.str();
    }

    void HandlerRegistry::push_history(const std::string& entry,
                                       HistoryStatus      status) {
        // Invariant: session_history_ always contains all commands issued in
        // this session, including those loaded from persistent storage at
        // startup. This method is the only place where new history entries are
        // appended.
        HistoryEntry new_entry;
        new_entry.command   = entry;
        new_entry.status    = status;
        new_entry.timestamp = get_current_timestamp();

        session_history_.emplace_back(new_entry);
        handlers::append_history_file(new_entry);

        // If rx_ is set, propagate the new entry to the external history
        // consumer.
        if (rx_ && !entry.empty()) {
            rx_->history_add(entry);
        }
    }

    void HandlerRegistry::visit(const SystemCommand& cmd) {
        // System commands may request process exit via should_exit_.
        // Handler must update last_command_status_ to reflect execution result.
        last_command_status_ =
            handlers::handle_system(cmd, ctx_, cfg_, should_exit_);
    }

    void HandlerRegistry::visit(const VarCommand& cmd) {
        // Variable commands are dispatched via var_reg_.
        // Correctness: var_reg_ must be initialized with all supported actions.
        last_command_status_ =
            var_reg_.dispatch(cmd.action(), cmd, ctx_, cfg_, current_env_);
    }

    void HandlerRegistry::visit(const MathCommand& cmd) {
        // Math commands are dispatched via math_reg_.
        // Assumes math_reg_ is populated for all MathCommand::Type variants.
        last_command_status_ =
            math_reg_.dispatch(cmd.type(), cmd, ctx_, cfg_, current_env_);
    }

    void HandlerRegistry::visit(const EnvCommand& cmd) {
        // Environment commands are dispatched via env_reg_.
        // Invariant: current_env_ must be valid for all env actions.
        last_command_status_ =
            env_reg_.dispatch(cmd.action(), cmd, ctx_, cfg_, current_env_);
    }

    void HandlerRegistry::visit(const ConfigCommand& cmd) {
        // Config commands are dispatched via config_reg_.
        // Note: config_reg_ is stateless and only mutates cfg_.
        last_command_status_ =
            config_reg_.dispatch(cmd.action(), cmd, ctx_, cfg_, current_env_);
    }

    void HandlerRegistry::visit(const LoadCommand& cmd) {
        // LoadCommand requires runner_ to be set; this is enforced here.
        // Handler is responsible for updating last_command_status_.
        if (!runner_) {
            throw std::runtime_error(
                "HandlerRegistry: Runner not set; cannot handle :load");
        }
        handlers::handle_load(cmd, *runner_);
        last_command_status_ = HistoryStatus::Success;
    }

    void HandlerRegistry::visit(const HistoryCommand& cmd) {
        // Special-case: HistoryCommand::Clear triggers both in-memory and
        // external clear. should_clear_history_ is set for deferred clearing.
        if (cmd.action() == HistoryCommand::Action::Clear) {
            should_clear_history_ = true;
            if (rx_)
                rx_->history_clear();
        }
        last_command_status_ = handlers::handle_history(cmd, session_history_);
    }

    void HandlerRegistry::visit(const RedoCommand& cmd) {
        // RedoCommand replays a previous command by parsing and dispatching it.
        // The lambda captures 'this' to allow recursive dispatch.
        // Correctness: parse_command must not mutate session state.
        last_command_status_ = handlers::handle_redo(
            cmd, session_history_, [this](const std::string& raw) {
                auto parsed = parse_command(raw);
                if (parsed)
                    dispatch(*parsed);
            });
    }

    HandlerRegistry build_handler_registry(Context&     ctx,
                                           Config&      config,
                                           std::string& current_env) {
        HandlerRegistry reg(ctx, config, current_env);

        // Load persisted history before registering handlers.
        // This ensures session_history_ is consistent with on-disk state.
        auto            persisted = handlers::load_history_file();
        for (const auto& entry : persisted) {
            reg.session_history_push_persisted(entry);
        }

        // Register variable command handlers.
        // Only Set and Unset actions are supported; others must be added
        // explicitly.
        for (auto type : {VarCommand::Action::Set, VarCommand::Action::Unset}) {
            reg.var().add(type,
                          [](const VarCommand& cmd,
                             Context&          ctx,
                             Config&           cfg,
                             std::string&      env) -> HistoryStatus {
                              return handlers::handle_var(cmd, ctx, cfg, env);
                          });
        }

        // Register math command handlers.
        // All supported MathCommand::Type variants must be listed here.
        for (auto type : {MathCommand::Type::Solve,
                          MathCommand::Type::Simplify,
                          MathCommand::Type::Expand,
                          MathCommand::Type::Factor,
                          MathCommand::Type::Evaluate}) {
            reg.math().add(type,
                           [](const MathCommand& cmd,
                              Context&           ctx,
                              Config&            cfg,
                              std::string& /*env*/) -> HistoryStatus {
                               return handlers::handle_math(cmd, ctx, cfg);
                           });
        }

        // Register environment command handlers.
        // All EnvCommand::Action variants are handled uniformly.
        for (auto action : {EnvCommand::Action::Show,
                            EnvCommand::Action::List,
                            EnvCommand::Action::Load,
                            EnvCommand::Action::Save,
                            EnvCommand::Action::New,
                            EnvCommand::Action::Delete,
                            EnvCommand::Action::Move,
                            EnvCommand::Action::Copy}) {
            reg.env().add(action,
                          [](const EnvCommand& cmd,
                             Context&          ctx,
                             Config&           cfg,
                             std::string&      env) -> HistoryStatus {
                              return handlers::handle_env(cmd, ctx, cfg, env);
                          });
        }

        // Register config command handlers.
        // Only ConfigCommand::Action variants listed here are supported.
        for (auto action : {ConfigCommand::Action::List,
                            ConfigCommand::Action::Get,
                            ConfigCommand::Action::Set,
                            ConfigCommand::Action::Path,
                            ConfigCommand::Action::Reset}) {
            reg.config_reg().add(action,
                                 [](const ConfigCommand& cmd,
                                    Context& /*ctx*/,
                                    Config& cfg,
                                    std::string& /*env*/) -> HistoryStatus {
                                     return handlers::handle_config(cmd, cfg);
                                 });
        }

        return reg;
    }

} // namespace math_solver