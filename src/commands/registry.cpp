#include "commands/registry.hpp"

#include "commands/handlers/config_handler.hpp"
#include "commands/handlers/env_handler.hpp"
#include "commands/handlers/math_handler.hpp"
#include "commands/handlers/system_handler.hpp"
#include "commands/handlers/var_handler.hpp"

namespace math_solver {

    // HandlerRegistry dispatches commands to the appropriate handler
    // subsystems. Invariant: All handler invocations must be side-effect free
    // except where explicitly intended (e.g., system exit, config mutation).
    // Correctness: The registry assumes that command objects are fully
    // validated before dispatch; no further validation is performed here.
    void HandlerRegistry::visit(const SystemCommand& cmd) {
        // System commands may request process exit; must update should_exit_
        // accordingly.
        should_exit_ = handlers::handle_system(cmd, ctx_, cfg_);
    }

    void HandlerRegistry::visit(const VarCommand& cmd) {
        // Variable registry dispatches based on action; assumes action is
        // valid.
        var_reg_.dispatch(cmd.action(), cmd, ctx_, cfg_, current_env_);
    }

    void HandlerRegistry::visit(const MathCommand& cmd) {
        // Math registry dispatches based on command type; see handler for side
        // effects.
        math_reg_.dispatch(cmd.type(), cmd, ctx_, cfg_, current_env_);
    }

    void HandlerRegistry::visit(const EnvCommand& cmd) {
        // Environment registry dispatches based on action; may mutate
        // current_env_.
        env_reg_.dispatch(cmd.action(), cmd, ctx_, cfg_, current_env_);
    }

    void HandlerRegistry::visit(const ConfigCommand& cmd) {
        // Config registry dispatches based on action; may mutate config.
        config_reg_.dispatch(cmd.action(), cmd, ctx_, cfg_, current_env_);
    }

    HandlerRegistry build_handler_registry(Context&     ctx,
                                           Config&      config,
                                           std::string& current_env) {
        HandlerRegistry reg(ctx, config, current_env);

        // Register variable command handlers.
        // Note: Set/Unset actions are assumed to be mutually exclusive.
        reg.var().add(
            VarCommand::Action::Set,
            [](const VarCommand& cmd,
               Context&          ctx,
               Config&           cfg,
               std::string& /*env*/) { handlers::handle_set(cmd, ctx, cfg); });

        reg.var().add(
            VarCommand::Action::Unset,
            [](const VarCommand& cmd,
               Context&          ctx,
               Config& /*cfg*/,
               std::string& /*env*/) { handlers::handle_unset(cmd, ctx); });

        // Register math command handlers.
        // Each math operation is assumed to be pure except where noted.
        reg.math().add(MathCommand::Type::Solve,
                       [](const MathCommand& cmd,
                          Context&           ctx,
                          Config&            cfg,
                          std::string& /*env*/) {
                           handlers::do_solve(cmd.payload(), ctx, cfg);
                       });

        reg.math().add(MathCommand::Type::Simplify,
                       [](const MathCommand& cmd,
                          Context&           ctx,
                          Config&            cfg,
                          std::string& /*env*/) {
                           handlers::do_simplify(cmd.payload(), cmd, ctx, cfg);
                       });

        reg.math().add(
            MathCommand::Type::Expand,
            [](const MathCommand& cmd,
               Context& /*ctx*/,
               Config& /*cfg*/,
               std::string& /*env*/) { handlers::do_expand(cmd.payload()); });

        reg.math().add(
            MathCommand::Type::Factor,
            [](const MathCommand& cmd,
               Context& /*ctx*/,
               Config& /*cfg*/,
               std::string& /*env*/) { handlers::do_factor(cmd.payload()); });

        reg.math().add(MathCommand::Type::Evaluate,
                       [](const MathCommand& cmd,
                          Context&           ctx,
                          Config&            cfg,
                          std::string& /*env*/) {
                           handlers::do_evaluate(cmd.payload(), ctx, cfg);
                       });

        // Register environment command handlers.
        // All actions are funneled to handle_env; correctness depends on
        // handler.
        for (auto action : {EnvCommand::Action::Show,
                            EnvCommand::Action::List,
                            EnvCommand::Action::Load,
                            EnvCommand::Action::Save,
                            EnvCommand::Action::New,
                            EnvCommand::Action::Delete}) {
            reg.env().add(action,
                          [](const EnvCommand& cmd,
                             Context&          ctx,
                             Config&           cfg,
                             std::string&      env) {
                              handlers::handle_env(cmd, ctx, cfg, env);
                          });
        }

        // Register config command handlers.
        // All config actions are handled via handle_config; assumes config is
        // mutable.
        for (auto action : {ConfigCommand::Action::List,
                            ConfigCommand::Action::Get,
                            ConfigCommand::Action::Set,
                            ConfigCommand::Action::Path,
                            ConfigCommand::Action::Reset}) {
            reg.config_reg().add(action,
                                 [](const ConfigCommand& cmd,
                                    Context& /*ctx*/,
                                    Config& cfg,
                                    std::string& /*env*/) {
                                     handlers::handle_config(cmd, cfg);
                                 });
        }

        // Registry is now fully constructed; all handlers must be registered
        // before use.
        return reg;
    }

} // namespace math_solver