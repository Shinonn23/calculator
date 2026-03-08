#include "command_parser_registry.hpp"
#include "parser/command/subparsers/history_command_parser.hpp"
#include "parser/command/subparsers/load_command_parser.hpp"
#include "subparsers/config_command_parser.hpp"
#include "subparsers/env_command_parser.hpp"
#include "subparsers/math_command_parser.hpp"
#include "subparsers/redo_command_parser.hpp"
#include "subparsers/system_command_parser.hpp"
#include "subparsers/var_command_parser.hpp"
#include <unordered_set>

namespace math_solver {

    SubparserRegistry build_registry() {
        SubparserRegistry                            reg;

        // Invariant: Each command string must be unique across all groups.
        // Collisions result in silent overwrites, breaking dispatch
        // correctness.
        // - Math commands: All symbolic manipulation commands must be routed to
        // MathCommandParser.
        //   The set below must be kept in sync with MathCommandParser's
        //   supported operations. Any divergence will cause dispatch failures
        //   for new/removed commands.
        static const std::unordered_set<std::string> math_cmds = {
            ":solve", ":simplify", ":expand", ":factor"};
        for (const auto& cmd : math_cmds)
            reg[cmd] = std::make_unique<MathCommandParser>();

        // Variable commands: Must remain disjoint from math_cmds to ensure
        // unambiguous dispatch. Overlap would result in last-writer-wins, which
        // is incorrect.
        static const std::unordered_set<std::string> var_cmds = {
            ":set", ":unset", ":rm"};
        for (const auto& cmd : var_cmds)
            reg[cmd] = std::make_unique<VarCommandParser>();

        // Env command: Singleton, intentionally not grouped.
        reg[":env"]     = std::make_unique<EnvCommandParser>();

        // Config commands: Both ":config" and ":conf" are supported for legacy
        // compatibility. Both must map to the same parser instance type.
        reg[":config"]  = std::make_unique<ConfigCommandParser>();
        reg[":conf"]    = std::make_unique<ConfigCommandParser>();

        // Load command: Not grouped; semantics are intentionally isolated.
        reg[":load"]    = std::make_unique<LoadCommandParser>();

        reg[":history"] = std::make_unique<HistoryCommandParser>();
        reg[":redo"]    = std::make_unique<RedoCommandParser>();

        // System commands: All side-effecting commands are grouped here.
        // No overlap with other command sets is allowed.
        // Any changes must be reflected in SystemCommandParser's dispatch
        // logic.
        static const std::unordered_set<std::string> sys_cmds = {
            ":exit", ":quit", ":q", ":help", ":h", ":clear", ":cls", ":ls"};
        for (const auto& cmd : sys_cmds)
            reg[cmd] = std::make_unique<SystemCommandParser>();

        // NOTE: The registry is the single source of truth for
        // command-to-parser mapping.
        //       Any additions/removals must be reflected both here and in the
        //       corresponding parser logic.
        return reg;
    }

    const std::unordered_set<std::string>& registered_command_names() {
        static const std::unordered_set<std::string> names = [] {
            auto                             reg = build_registry();
            std::unordered_set<std::string> s;
            s.reserve(reg.size());
            for (const auto& [k, _] : reg)
                s.insert(k);
            return s;
        }();
        return names;
    }

} // namespace math_solver
