#pragma once

#include "ast/command/command.hpp"
#include "ast/command/command_visitor.hpp"
#include "ast/command/config_command.hpp"
#include "ast/command/env_command.hpp"
#include "ast/command/history_command.hpp"
#include "ast/command/history_entry.hpp"
#include "ast/command/load_command.hpp"
#include "ast/command/math_command.hpp"
#include "ast/command/redo_command.hpp"
#include "ast/command/system_command.hpp"
#include "ast/command/var_command.hpp"
#include "config/config.hpp"
#include "diagnostics/sink.hpp"
#include "replxx.hxx"
#include "runtime/context/context.hpp"

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace math_solver {

    // Generic dispatch table for mapping command keys to handler closures.
    //
    // Invariants:
    // - All valid keys must be registered before any dispatch occurs.
    // - No synchronization; single-threaded usage is assumed.
    // - Handler ownership is external; registry does not manage handler
    // lifetimes.
    // - Duplicate key registration overwrites previous handler (intentional).
    //
    // Failure to find a key during dispatch is fatal; this is relied upon to
    // catch programming errors early.
    template <typename Cmd, typename Key> class CommandRegistry {
        public:
        using Handler = std::function<HistoryStatus(
            const Cmd&, Context&, Config&, std::string& /*current_env*/,
            DiagnosticSink& /*sink*/)>;

        void add(Key key, Handler handler) {
            handlers_[key] = std::move(handler);
        }

        // Returns handler result for the given key.
        // Precondition: key must be registered; otherwise, aborts.
        HistoryStatus dispatch(Key key, const Cmd& cmd, Context& ctx,
                               Config& config, std::string& current_env,
                               DiagnosticSink& sink) const {
            auto it = handlers_.find(key);
            if (it == handlers_.end()) {
                sink.push(
                    Diagnostic::make("CommandRegistry: unregistered key " +
                                         std::to_string(static_cast<int>(key)),
                                     "E9999", Span{}));
                return HistoryStatus::Error;
            }
            return it->second(cmd, ctx, config, current_env, sink);
        }

        bool has(Key key) const { return handlers_.count(key) != 0; }

        private:
        std::unordered_map<Key, Handler> handlers_;
    };

    class Runner;

    // Centralizes dispatch for all command kinds.
    //
    // Invariants:
    // - Each command kind has a dedicated registry instance.
    // - Not thread-safe; all mutation and dispatch must be single-threaded.
    // - Context, Config, and current_env references must outlive this registry.
    //
    // History management:
    // - session_history_ is the canonical in-memory history for the session.
    // - Only push_history() mutates session_history_ (except for loading
    // persisted entries).
    // - should_clear_history_ is set by handlers to signal the main loop to
    // clear history.
    //
    // Interactions:
    // - REPL integration via replxx::Replxx* for history replay.
    // - Runner* is set externally for command handlers that require it.
    //
    // Correctness:
    // - All command dispatches must go through this registry to ensure
    //   invariants are maintained.
    class HandlerRegistry : public CommandVisitor {
        public:
        using SystemReg = CommandRegistry<SystemCommand, SystemCommand::Type>;
        using VarReg    = CommandRegistry<VarCommand, VarCommand::Action>;
        using MathReg   = CommandRegistry<MathCommand, MathCommand::Type>;
        using EnvReg    = CommandRegistry<EnvCommand, EnvCommand::Action>;
        using ConfigReg = CommandRegistry<ConfigCommand, ConfigCommand::Action>;
        using HistoryReg =
            CommandRegistry<HistoryCommand, HistoryCommand::Action>;
        using RedoReg = CommandRegistry<RedoCommand, std::string /*range*/>;

        HandlerRegistry(Context& ctx, Config& config, std::string& current_env,
                        DiagnosticSink& sink)
            : ctx_(ctx), cfg_(config), current_env_(current_env), sink_(sink),
              should_exit_(false), should_clear_history_(false) {}

        SystemReg&  system() { return system_reg_; }
        VarReg&     var() { return var_reg_; }
        MathReg&    math() { return math_reg_; }
        EnvReg&     env() { return env_reg_; }
        ConfigReg&  config_reg() { return config_reg_; }
        HistoryReg& history() { return history_reg_; }
        RedoReg&    redo() { return redo_reg_; }

        // Appends entry to in-memory history with status.
        // Only called by the main loop after each dispatch.
        void push_history(const std::string& entry, HistoryStatus status);

        // Clears in-memory history. Only valid to call if should_clear_history_
        // is set.
        void clear_history() {
            session_history_.clear();
            should_clear_history_ = false;
        }

        const std::vector<HistoryEntry>& session_history() const {
            return session_history_;
        }

        // Returns false if the command signals process exit.
        // Main loop must call clear_history() if should_clear_history_ is set.
        // Postcondition: last_command_status_ is Warning if the handler
        // succeeded but warnings were emitted, so push_history() records the
        // correct label.
        bool dispatch(const Command& cmd, DiagnosticSink& sink) {
            should_exit_          = false;
            should_clear_history_ = false;
            cmd.accept(*this, sink);
            if (last_command_status_ == HistoryStatus::Success &&
                sink.has_warnings())
                last_command_status_ = HistoryStatus::Warning;
            return !should_exit_;
        }

        bool dispatch(const Command& cmd) { return dispatch(cmd, sink_); }

        bool should_clear_history() const { return should_clear_history_; }

        // Used for loading persisted history entries into the current session.
        // Only called during startup or history replay.
        void session_history_push_persisted(const HistoryEntry& entry) {
            session_history_.push_back(entry);
        }

        HistoryStatus last_command_status() const {
            return last_command_status_;
        }
        void set_replxx(replxx::Replxx& rx) { rx_ = &rx; }

        // Loads session history into the REPL backend for up-arrow recall.
        // Only entries with non-empty commands are added.
        void load_persisted_history() {
            if (!rx_)
                return;
            for (const auto& entry : session_history_) {
                if (!entry.command.empty()) {
                    rx_->history_add(entry.command);
                }
            }
        }

        const std::string& current_env() const { return current_env_; }
        std::string&       current_env_mut() { return current_env_; }

        Context&           ctx() { return ctx_; }
        Config&            config() { return cfg_; }

        void               set_runner(Runner& runner) { runner_ = &runner; }
        DiagnosticSink&    sink() { return sink_; }

        // CommandVisitor overrides. Each handler is responsible for updating
        // last_command_status_, should_exit_, and should_clear_history_ as
        // needed.
        void visit(const SystemCommand& cmd, DiagnosticSink& sink) override;
        void visit(const VarCommand& cmd, DiagnosticSink& sink) override;
        void visit(const MathCommand& cmd, DiagnosticSink& sink) override;
        void visit(const EnvCommand& cmd, DiagnosticSink& sink) override;
        void visit(const ConfigCommand& cmd, DiagnosticSink& sink) override;
        void visit(const LoadCommand& cmd, DiagnosticSink& sink) override;
        void visit(const HistoryCommand& cmd, DiagnosticSink& sink) override;
        void visit(const RedoCommand& cmd, DiagnosticSink& sink) override;

        private:
        Context&                  ctx_;
        Config&                   cfg_;
        std::string&              current_env_;
        DiagnosticSink&           sink_;
        bool                      should_exit_;
        bool                      should_clear_history_;
        HistoryStatus             last_command_status_ = HistoryStatus::Success;

        SystemReg                 system_reg_;
        VarReg                    var_reg_;
        MathReg                   math_reg_;
        EnvReg                    env_reg_;
        ConfigReg                 config_reg_;
        HistoryReg                history_reg_;
        RedoReg                   redo_reg_;

        // Canonical in-memory history for the current session.
        // Only mutated via push_history() and session_history_push_persisted().
        std::vector<HistoryEntry> session_history_;

        Runner*                   runner_ = nullptr;
        replxx::Replxx*           rx_     = nullptr;
    };

    HandlerRegistry build_handler_registry(Context& ctx, Config& config,
                                           std::string&    current_env,
                                           DiagnosticSink& sink);

} // namespace math_solver