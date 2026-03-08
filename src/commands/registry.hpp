#pragma once

//! # Module — `src/commands/registry.hpp`
//!
//! Defines `CommandRegistry<Cmd, Key>` — a generic dispatch table that maps
//! command-type keys to handler closures — and `HandlerRegistry`, the central
//! `CommandVisitor` implementation that owns one `CommandRegistry` per command
//! kind, manages the in-session command history, and drives REPL integration.
//!
//! The free function `build_handler_registry` constructs and wires a fully
//! initialised `HandlerRegistry` ready for use by the REPL main loop.

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

    /// Generic dispatch table that maps command-type keys to handler closures.
    ///
    /// Invariants:
    /// - All valid keys must be registered before any dispatch occurs.
    /// - No synchronization; single-threaded usage is assumed.
    /// - Handler ownership is external; the registry does not manage handler
    ///   lifetimes.
    /// - Duplicate key registration overwrites the previous handler.
    template <typename Cmd, typename Key> class CommandRegistry {
        public:
        /// Callable type stored per key; signature matches the handler
        /// convention expected by `HandlerRegistry::visit` overrides.
        using Handler = std::function<HistoryStatus(
            const Cmd&, Context&, Config&, std::string& /*current_env*/,
            DiagnosticSink& /*sink*/)>;

        /// Register `handler` for `key`, overwriting any previous registration.
        ///
        /// # Arguments
        ///
        /// * `key`     — The command-type discriminant to register.
        /// * `handler` — The closure to invoke when `key` is dispatched.
        void add(Key key, Handler handler) {
            handlers_[key] = std::move(handler);
        }

        /// Invoke the handler registered for `key` and return its status.
        ///
        /// Pushes an E9999 diagnostic and returns `HistoryStatus::Error` when
        /// `key` is not registered.
        ///
        /// # Arguments
        ///
        /// * `key`         — Discriminant identifying the handler to invoke.
        /// * `cmd`         — The command node to forward to the handler.
        /// * `ctx`         — Live variable context passed through to the handler.
        /// * `config`      — Live configuration passed through to the handler.
        /// * `current_env` — Name of the active environment.
        /// * `sink`        — Diagnostic sink for errors and output.
        ///
        /// # Returns
        ///
        /// The `HistoryStatus` produced by the registered handler, or
        /// `HistoryStatus::Error` if the key is not registered.
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

        /// Return `true` if a handler is registered for `key`.
        bool has(Key key) const { return handlers_.count(key) != 0; }

        private:
        std::unordered_map<Key, Handler> handlers_;
    };

    class Runner;

    /// Central `CommandVisitor` that owns one `CommandRegistry` per command
    /// kind and drives the REPL command dispatch loop.
    ///
    /// Invariants:
    /// - Each command kind has a dedicated registry instance.
    /// - Not thread-safe; all mutation and dispatch must be single-threaded.
    /// - `ctx_`, `cfg_`, and `current_env_` references must outlive this
    ///   registry.
    ///
    /// History:
    /// - `session_history_` is the canonical in-memory history for the session.
    /// - Only `push_history()` and `session_history_push_persisted()` mutate it.
    /// - `should_clear_history_` is set by handlers to signal the main loop.
    ///
    /// Interactions:
    /// - `Runner*` must be set via `set_runner()` before `:load` commands are
    ///   dispatched.
    /// - `replxx::Replxx*` must be set via `set_replxx()` for REPL history.
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

        /// Construct a `HandlerRegistry` bound to the given live references.
        ///
        /// # Arguments
        ///
        /// * `ctx`         — Variable context for this session.
        /// * `config`      — Configuration store for this session.
        /// * `current_env` — Name of the currently active environment.
        /// * `sink`        — Default diagnostic sink used by `dispatch(cmd)`.
        HandlerRegistry(Context& ctx, Config& config, std::string& current_env,
                        DiagnosticSink& sink)
            : ctx_(ctx), cfg_(config), current_env_(current_env), sink_(sink),
              should_exit_(false), should_clear_history_(false) {}

        /// Return a mutable reference to the system-command sub-registry.
        SystemReg&  system() { return system_reg_; }
        /// Return a mutable reference to the variable-command sub-registry.
        VarReg&     var() { return var_reg_; }
        /// Return a mutable reference to the math-command sub-registry.
        MathReg&    math() { return math_reg_; }
        /// Return a mutable reference to the environment-command sub-registry.
        EnvReg&     env() { return env_reg_; }
        /// Return a mutable reference to the config-command sub-registry.
        ConfigReg&  config_reg() { return config_reg_; }
        /// Return a mutable reference to the history-command sub-registry.
        HistoryReg& history() { return history_reg_; }
        /// Return a mutable reference to the redo-command sub-registry.
        RedoReg&    redo() { return redo_reg_; }

        /// Append an entry to the in-memory session history and persist it.
        ///
        /// Also forwards the entry to the replxx backend when set. This is the
        /// sole mutation point for `session_history_`; call it once per
        /// dispatched command from the main loop.
        ///
        /// # Arguments
        ///
        /// * `entry`  — The raw command string that was executed.
        /// * `status` — The outcome status to record.
        void push_history(const std::string& entry, HistoryStatus status);

        /// Clear the in-memory session history.
        ///
        /// Only valid to call when `should_clear_history()` is `true`; resets
        /// the flag.
        void clear_history() {
            session_history_.clear();
            should_clear_history_ = false;
        }

        /// Return the full in-memory session history.
        const std::vector<HistoryEntry>& session_history() const {
            return session_history_;
        }

        /// Dispatch `cmd` through the visitor, returning `false` if the command
        /// signals process exit.
        ///
        /// After dispatch, upgrades `last_command_status_` from `Success` to
        /// `Warning` when the sink has accumulated warnings, so that
        /// `push_history` records the correct label. The caller must check
        /// `should_clear_history()` and call `clear_history()` if set.
        ///
        /// # Arguments
        ///
        /// * `cmd`  — The command node to execute.
        /// * `sink` — Diagnostic sink to use for this command.
        ///
        /// # Returns
        ///
        /// `false` when the command type is `:exit` / `:quit`; `true`
        /// otherwise.
        bool dispatch(const Command& cmd, DiagnosticSink& sink) {
            should_exit_          = false;
            should_clear_history_ = false;
            cmd.accept(*this, sink);
            if (last_command_status_ == HistoryStatus::Success &&
                sink.has_warnings())
                last_command_status_ = HistoryStatus::Warning;
            return !should_exit_;
        }

        /// Dispatch `cmd` using the default diagnostic sink bound at
        /// construction.
        bool dispatch(const Command& cmd) { return dispatch(cmd, sink_); }

        /// Return `true` if a handler requested that the history be cleared.
        bool should_clear_history() const { return should_clear_history_; }

        /// Insert a pre-existing history entry without mutating the on-disk
        /// file.
        ///
        /// Used during startup to populate `session_history_` from the
        /// persisted history file before normal dispatch begins.
        void session_history_push_persisted(const HistoryEntry& entry) {
            session_history_.push_back(entry);
        }

        /// Return the status recorded for the most recently dispatched command.
        HistoryStatus last_command_status() const {
            return last_command_status_;
        }

        /// Attach a `replxx::Replxx` instance for REPL history integration.
        void set_replxx(replxx::Replxx& rx) { rx_ = &rx; }

        /// Forward all non-empty session history entries to the replxx backend.
        ///
        /// Must be called after `session_history_push_persisted` has populated
        /// the history, before the REPL main loop starts.
        void load_persisted_history() {
            if (!rx_)
                return;
            for (const auto& entry : session_history_) {
                if (!entry.command.empty()) {
                    rx_->history_add(entry.command);
                }
            }
        }

        /// Return the name of the currently active environment.
        const std::string& current_env() const { return current_env_; }

        /// Return a mutable reference to the active environment name.
        std::string&       current_env_mut() { return current_env_; }

        /// Return a mutable reference to the live variable context.
        Context&           ctx() { return ctx_; }

        /// Return a mutable reference to the live configuration.
        Config&            config() { return cfg_; }

        /// Set the `Runner` instance required for `:load` command dispatch.
        ///
        /// Must be called before any `LoadCommand` is dispatched.
        void               set_runner(Runner& runner) { runner_ = &runner; }

        /// Return the default diagnostic sink bound at construction.
        DiagnosticSink&    sink() { return sink_; }

        /// Dispatch a `SystemCommand` (exit, help, clear, ls) via the handler.
        void visit(const SystemCommand& cmd, DiagnosticSink& sink) override;
        /// Dispatch a `VarCommand` (set, unset) via the var sub-registry.
        void visit(const VarCommand& cmd, DiagnosticSink& sink) override;
        /// Dispatch a `MathCommand` (solve, simplify, expand, factor, evaluate)
        /// via the math sub-registry.
        void visit(const MathCommand& cmd, DiagnosticSink& sink) override;
        /// Dispatch an `EnvCommand` (show, list, load, save, …) via the env
        /// sub-registry.
        void visit(const EnvCommand& cmd, DiagnosticSink& sink) override;
        /// Dispatch a `ConfigCommand` (list, get, set, path, reset) via the
        /// config sub-registry.
        void visit(const ConfigCommand& cmd, DiagnosticSink& sink) override;
        /// Dispatch a `LoadCommand` by forwarding to the `Runner`.
        void visit(const LoadCommand& cmd, DiagnosticSink& sink) override;
        /// Dispatch a `HistoryCommand` (show, search, save, clear) and
        /// optionally set `should_clear_history_`.
        void visit(const HistoryCommand& cmd, DiagnosticSink& sink) override;
        /// Dispatch a `RedoCommand` by re-parsing and re-dispatching history
        /// entries.
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

    /// Construct and fully initialise a `HandlerRegistry`.
    ///
    /// Loads the persisted history file, then registers all handler closures
    /// for every supported command type. The returned registry is ready for
    /// use by the REPL main loop.
    ///
    /// # Arguments
    ///
    /// * `ctx`         — Live variable context.
    /// * `config`      — Live configuration store.
    /// * `current_env` — Active environment name; mutated on `:env load`.
    /// * `sink`        — Default diagnostic sink.
    ///
    /// # Returns
    ///
    /// A `HandlerRegistry` with all sub-registries populated.
    HandlerRegistry build_handler_registry(Context& ctx, Config& config,
                                           std::string&    current_env,
                                           DiagnosticSink& sink);

} // namespace math_solver