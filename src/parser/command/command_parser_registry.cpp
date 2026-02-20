#include "command_parser_registry.hpp"
#include "parser/command/subparsers/load_command_parser.hpp"
#include "subparsers/config_command_parser.hpp"
#include "subparsers/env_command_parser.hpp"
#include "subparsers/math_command_parser.hpp"
#include "subparsers/system_command_parser.hpp"
#include "subparsers/var_command_parser.hpp"
#include <unordered_set>

namespace math_solver {

    SubparserRegistry build_registry() {
        SubparserRegistry                            reg;

        // Math commands: All symbolic manipulation commands must be routed to
        // MathCommandParser. The set below must remain in sync with the
        // supported operations in MathCommandParser; divergence will cause
        // parser dispatch to fail for new or removed commands. No overlap with
        // other command groups is permitted.
        static const std::unordered_set<std::string> math_cmds = {
            ":solve", ":simplify", ":expand", ":factor"};
        for (const auto& cmd : math_cmds)
            reg[cmd] = std::make_unique<MathCommandParser>();

        // Variable commands: These must remain disjoint from math_cmds to
        // guarantee unambiguous dispatch. Any overlap will result in
        // last-writer-wins, which is incorrect. See VarCommandParser for
        // semantics.
        static const std::unordered_set<std::string> var_cmds = {
            ":set", ":unset", ":rm"};
        for (const auto& cmd : var_cmds)
            reg[cmd] = std::make_unique<VarCommandParser>();

        // Env command: Singleton, not grouped. Semantics are intentionally
        // distinct from other command classes.
        reg[":env"]    = std::make_unique<EnvCommandParser>();

        // Config commands: Both ":config" and ":conf" are supported for
        // compatibility with legacy scripts. Both must map to the same parser.
        reg[":config"] = std::make_unique<ConfigCommandParser>();
        reg[":conf"]   = std::make_unique<ConfigCommandParser>();

        // Load command: Not grouped; semantics are intentionally isolated.
        reg[":load"]   = std::make_unique<LoadCommandParser>();

        // System commands: All side-effecting commands are grouped here.
        // No overlap with other command sets is allowed. Any changes must be
        // reflected in SystemCommandParser's dispatch logic.
        static const std::unordered_set<std::string> sys_cmds = {
            ":exit", ":quit", ":q", ":help", ":h", ":clear", ":cls", ":ls"};
        for (const auto& cmd : sys_cmds)
            reg[cmd] = std::make_unique<SystemCommandParser>();

        // Invariant: All command strings must be unique across all groups.
        // Collisions will silently overwrite previous entries, leading to
        // incorrect parser dispatch. This must be maintained as a hard
        // requirement.
        return reg;
    }

} // namespace math_solver
