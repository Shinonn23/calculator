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

    // Generic registry mapping command keys to handler functions.
    //
    // - Key must be hashable and suitable for use in unordered_map.
    // - No synchronization; intended for single-threaded use only.
    // - Handler lifetime is managed externally; registry does not own
    // resources.
    // - Overwrites on duplicate key are silent and intentional.
    // - All valid keys must be registered before dispatch; missing keys are
    // fatal.
    // - Used as a building block for higher-level command dispatch.
    template <typename Cmd, typename Key> class CommandRegistry {
        public:
        using Handler = std::function<void(
            const Cmd&, Context&, Config&, std::string& /*current_env*/)>;

        void add(Key key, Handler handler) {
            handlers_[key] = std::move(handler);
        }

        // Panics if key is not registered. This is a hard error: all valid keys
        // must be registered prior to dispatch. No fallback or recovery.
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

        bool has(Key key) const { return handlers_.count(key) != 0; }

        private:
        std::unordered_map<Key, Handler> handlers_;
    };

    class Runner;

    // Centralized handler registry for all command kinds.
    //
    // - Each command kind has a dedicated registry instance.
    // - Not thread-safe; all mutation and dispatch must be single-threaded.
    // - Context, Config, and current_env references must outlive this registry.
    // - should_exit_ is used for early termination signaling (e.g., after
    // exit).
    // - Interacts with Runner for commands that require orchestration.
    // - Assumes that command accept() will invoke the correct visit() overload.
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

        // Returns false if the command signals process exit (see should_exit_).
        // All command dispatches are expected to be side-effecting.
        bool       dispatch(const Command& cmd) {
            should_exit_ = false;
            cmd.accept(*this);
            return !should_exit_;
        }

        void set_runner(Runner& runner) { runner_ = &runner; }

        void visit(const SystemCommand& cmd) override;
        void visit(const VarCommand& cmd) override;
        void visit(const MathCommand& cmd) override;
        void visit(const EnvCommand& cmd) override;
        void visit(const ConfigCommand& cmd) override;
        void visit(const LoadCommand& cmd) override;

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

        Runner*      runner_ = nullptr;
    };

    // Constructs a HandlerRegistry with all required dependencies.
    //
    // - Registry is returned in an unpopulated state; caller is responsible for
    //   registering all handlers before use.
    // - Lifetime of ctx, config, and current_env must exceed that of the
    // registry.
    HandlerRegistry build_handler_registry(Context&     ctx,
                                           Config&      config,
                                           std::string& current_env);

} // namespace math_solver
