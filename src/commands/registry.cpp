#include "commands/registry.hpp"

#include "ast/command/math_command.hpp"
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

    // Relies on system clock; precision is limited by
    // std::chrono::system_clock. Used for ordering and deduplication in
    // history. Any change to timestamp format must be coordinated with
    // persistent storage consumers.
    static std::string get_current_timestamp() {
        auto now        = std::chrono::system_clock::now();
        auto time_t_now = std::chrono::system_clock::to_time_t(now);
        auto ms         = std::chrono::duration_cast<std::chrono::milliseconds>(
                      now.time_since_epoch()) %
                  1000;

        std::stringstream ss;
#if defined(__unix__) || defined(__APPLE__)
        struct tm  buf;
        struct tm* timeinfo = localtime_r(&time_t_now, &buf);
#else
        struct tm* timeinfo = std::localtime(&time_t_now);
#endif
        ss << std::put_time(timeinfo, "%Y-%m-%d %H:%M:%S") << '.'
           << std::setfill('0') << std::setw(3) << ms.count();
        return ss.str();
    }

    void HandlerRegistry::push_history(const std::string& entry,
                                       HistoryStatus      status) {
        // Only entry point for mutating session_history_.
        // - session_history_ must remain consistent with on-disk history.
        // - If rx_ is set, external consumers must be notified of all changes.
        // - Invariant: entry must be non-empty for rx_ propagation.
        HistoryEntry new_entry;
        new_entry.command   = entry;
        new_entry.status    = status;
        new_entry.timestamp = get_current_timestamp();

        session_history_.emplace_back(new_entry);
        handlers::append_history_file(new_entry);

        if (rx_ && !entry.empty()) {
            rx_->history_add(entry);
        }
    }

    void HandlerRegistry::visit(const SystemCommand& cmd,
                                DiagnosticSink&      sink) {
        // System commands may request process exit via should_exit_.
        // last_command_status_ must always reflect the result of handler.
        last_command_status_ =
            handlers::handle_system(cmd, ctx_, cfg_, should_exit_, sink);
    }

    void HandlerRegistry::visit(const VarCommand& cmd, DiagnosticSink& sink) {
        // All variable command actions must be registered in var_reg_.
        // Correctness: var_reg_ must not be mutated concurrently.
        last_command_status_ = var_reg_.dispatch(cmd.action(), cmd, ctx_, cfg_,
                                                 current_env_, sink);
    }

    void HandlerRegistry::visit(const MathCommand& cmd, DiagnosticSink& sink) {
        // math_reg_ must be fully populated for all MathCommand::Type variants.
        // Any missing handler is a logic error.
        last_command_status_ =
            math_reg_.dispatch(cmd.type(), cmd, ctx_, cfg_, current_env_, sink);
    }

    void HandlerRegistry::visit(const EnvCommand& cmd, DiagnosticSink& sink) {
        // env_reg_ must be initialized for all EnvCommand::Action variants.
        // current_env_ must be valid for all env actions.
        last_command_status_ = env_reg_.dispatch(cmd.action(), cmd, ctx_, cfg_,
                                                 current_env_, sink);
    }

    void HandlerRegistry::visit(const ConfigCommand& cmd,
                                DiagnosticSink&      sink) {
        // config_reg_ is stateless; only mutates cfg_.
        // All supported ConfigCommand::Action variants must be registered.
        last_command_status_ = config_reg_.dispatch(cmd.action(), cmd, ctx_,
                                                    cfg_, current_env_, sink);
    }

    void HandlerRegistry::visit(const LoadCommand& cmd, DiagnosticSink& sink) {
        // runner_ must be set prior to handling LoadCommand.
        // If runner_ is unset, this is a fatal logic error.
        if (!runner_) {
            throw std::runtime_error(
                "HandlerRegistry: Runner not set; cannot handle :load");
        }
        handlers::handle_load(cmd, *runner_, sink);
        last_command_status_ = HistoryStatus::Success;
    }

    void HandlerRegistry::visit(const HistoryCommand& cmd,
                                DiagnosticSink&       sink) {
        // HistoryCommand::Clear triggers both in-memory and external clear.
        // should_clear_history_ is set for deferred clearing.
        // rx_ must be notified if present.
        if (cmd.action() == HistoryCommand::Action::Clear) {
            should_clear_history_ = true;
            if (rx_)
                rx_->history_clear();
        }
        last_command_status_ =
            handlers::handle_history(cmd, session_history_, sink);
    }

    void HandlerRegistry::visit(const RedoCommand& cmd, DiagnosticSink& sink) {
        // RedoCommand replays a previous command by parsing and dispatching it.
        // - parse_command must not mutate session state.
        // - Recursive dispatch is permitted; correctness relies on
        // parse_command
        //   being side-effect free.
        last_command_status_ = handlers::handle_redo(
            cmd, session_history_,
            [this, &sink](const std::string& raw) {
                auto parse_result = parse_command(raw);
                if (parse_result)
                    dispatch(*std::move(*parse_result), sink);
            },
            sink);
    }

    HandlerRegistry build_handler_registry(Context& ctx, Config& config,
                                           std::string&    current_env,
                                           DiagnosticSink& sink) {
        HandlerRegistry reg(ctx, config, current_env, sink);

        // Load persisted history before registering handlers.
        // - Ensures session_history_ is consistent with on-disk state.
        // - Must be called before any command handlers are registered.
        auto            persisted = handlers::load_history_file();
        for (const auto& entry : persisted) {
            reg.session_history_push_persisted(entry);
        }

        // Register variable command handlers.
        // - Only Set, Unset, and Unknown actions are supported here.
        // - Any new VarCommand::Action must be explicitly registered.
        for (auto type : {VarCommand::Action::Set, VarCommand::Action::Unset,
                          VarCommand::Action::Unknown}) {
            reg.var().add(
                type,
                [](const VarCommand& cmd, Context& ctx, Config& cfg,
                   std::string& env, DiagnosticSink& sink) -> HistoryStatus {
                    return handlers::handle_var(cmd, ctx, cfg, env, sink);
                });
        }

        // Register math command handlers.
        // - All supported MathCommand::Type variants must be listed.
        // - Any omission is a logic error.
        for (auto type :
             {MathCommand::Type::Solve, MathCommand::Type::Simplify,
              MathCommand::Type::Expand, MathCommand::Type::Factor,
              MathCommand::Type::Evaluate, MathCommand::Type::Unknown}) {
            reg.math().add(type,
                           [](const MathCommand& cmd, Context& ctx, Config& cfg,
                              std::string& /*env*/,
                              DiagnosticSink& sink) -> HistoryStatus {
                               return handlers::handle_math(cmd, ctx, cfg,
                                                            sink);
                           });
        }

        // Register environment command handlers.
        // - All EnvCommand::Action variants are handled uniformly.
        // - Any missing action is a logic error.
        for (auto action : {EnvCommand::Action::Show, EnvCommand::Action::List,
                            EnvCommand::Action::Load, EnvCommand::Action::Save,
                            EnvCommand::Action::New, EnvCommand::Action::Delete,
                            EnvCommand::Action::Move, EnvCommand::Action::Copy,
                            EnvCommand::Action::Unknown}) {
            reg.env().add(
                action,
                [](const EnvCommand& cmd, Context& ctx, Config& cfg,
                   std::string& env, DiagnosticSink& sink) -> HistoryStatus {
                    return handlers::handle_env(cmd, ctx, cfg, env, sink);
                });
        }

        // Register config command handlers.
        // - Only ConfigCommand::Action variants listed here are supported.
        // - Any new action must be registered explicitly.
        for (auto action :
             {ConfigCommand::Action::List, ConfigCommand::Action::Get,
              ConfigCommand::Action::Set, ConfigCommand::Action::Path,
              ConfigCommand::Action::Reset, ConfigCommand::Action::Unknown}) {
            reg.config_reg().add(action,
                                 [](const ConfigCommand& cmd, Context& /*ctx*/,
                                    Config&         cfg, std::string& /*env*/,
                                    DiagnosticSink& sink) -> HistoryStatus {
                                     return handlers::handle_config(cmd, cfg,
                                                                    sink);
                                 });
        }

        return reg;
    }

} // namespace math_solver