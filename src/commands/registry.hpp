#pragma once

#include "ast/command/command.hpp"
#include "ast/command/command_visitor.hpp"
#include "ast/command/config_command.hpp"
#include "ast/command/env_command.hpp"
#include "ast/command/math_command.hpp"
#include "ast/command/system_command.hpp"
#include "ast/command/var_command.hpp"
#include "config/config.hpp"
#include "runtime/context/context.hpp"

#include <functional>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace math_solver {
    // Generic registry for mapping command keys to handler functions.
    // - Key must be hashable and suitable for use in unordered_map.
    // - Handler is responsible for all side effects; registry does not enforce
    // any invariants on handler behavior.
    // - No synchronization; intended for single-threaded use.
    template <typename Cmd, typename Key> class CommandRegistry {
        public:
        using Handler = std::function<void(
            const Cmd&, Context&, Config&, std::string& /*current_env*/)>;

        // Overwrites any existing handler for the given key.
        void add(Key key, Handler handler) {
            handlers_[key] = std::move(handler);
        }

        // Dispatches to the registered handler for `key`.
        // Panics (throws) if no handler is registered.
        // - Assumes that all valid keys are registered before dispatch.
        // - The error message includes the integer value of the key for
        // diagnostics.
        void dispatch(Key          key,
                      const Cmd&   cmd,
                      Context&     ctx,
                      Config&      config,
                      std::string& current_env) const {
            auto it = handlers_.find(key);
            if (it == handlers_.end())
                throw std::out_of_range("CommandRegistry: unregistered key " +
                                        std::to_string(static_cast<int>(key)));
            it->second(cmd, ctx, config, current_env);
        }

        // Returns true if a handler is registered for `key`.
        bool has(Key key) const { return handlers_.count(key) != 0; }

        private:
        std::unordered_map<Key, Handler> handlers_;
    };

    // Central registry for all command handlers.
    // - Maintains one registry per command kind.
    // - Lifetime of referenced Context, Config, and current_env must outlive
    // this registry.
    // - Not thread-safe; all mutation and dispatch is expected to be
    // single-threaded.
    // - `should_exit_` is used to signal early termination (e.g., after an exit
    // command).
    class HandlerRegistry : public CommandVisitor {
        public:
        using SystemReg = CommandRegistry<SystemCommand, SystemCommand::Type>;
        using VarReg    = CommandRegistry<VarCommand, VarCommand::Action>;
        using MathReg   = CommandRegistry<MathCommand, MathCommand::Type>;
        using EnvReg    = CommandRegistry<EnvCommand, EnvCommand::Action>;
        using ConfigReg = CommandRegistry<ConfigCommand, ConfigCommand::Action>;

        HandlerRegistry(Context& ctx, Config& config, std::string& current_env)
            : ctx_(ctx),
              cfg_(config),
              current_env_(current_env),
              should_exit_(false) {}

        SystemReg& system() { return system_reg_; }
        VarReg&    var() { return var_reg_; }
        MathReg&   math() { return math_reg_; }
        EnvReg&    env() { return env_reg_; }
        ConfigReg& config_reg() { return config_reg_; }

        // Dispatches the given command to the appropriate handler.
        // - Returns false if the command signals process exit (see
        // should_exit_).
        // - Assumes that the command's accept() will invoke the correct visit()
        // overload.
        bool       dispatch(const Command& cmd) {
            should_exit_ = false;
            cmd.accept(*this);
            return !should_exit_;
        }

        void visit(const SystemCommand& cmd) override;
        void visit(const VarCommand& cmd) override;
        void visit(const MathCommand& cmd) override;
        void visit(const EnvCommand& cmd) override;
        void visit(const ConfigCommand& cmd) override;

        private:
        Context&     ctx_;
        Config&      cfg_;
        std::string& current_env_;
        bool         should_exit_;

        SystemReg    system_reg_;
        VarReg       var_reg_;
        MathReg      math_reg_;
        EnvReg       env_reg_;
        ConfigReg    config_reg_;
    };

    // Constructs a HandlerRegistry with all required dependencies.
    // - The returned registry is expected to be further populated with handlers
    // by the caller.
    HandlerRegistry build_handler_registry(Context&     ctx,
                                           Config&      config,
                                           std::string& current_env);

} // namespace math_solver