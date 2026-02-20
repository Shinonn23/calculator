#ifndef COMMAND_AST_H
#define COMMAND_AST_H
#include <optional>
#include <string>
#include <vector>

namespace math_solver {

    // ── Command type ────────────────────────────────────────────────────
    // Every CLI command maps to exactly one enum value.
    // The parser resolves aliases (e.g. "h" → Help, "cls" → Clear).

    enum class CommandType {
        // Meta
        Exit,
        Help,
        Unknown,

        // Variables
        Set,
        Unset,
        Clear,
        Ls,

        // Math operations
        Solve,
        Simplify,
        Expand,
        Factor,

        // Config / Environment
        Config,
        Env,

        // Math expression (not a CLI command — bare input)
        MathExpr,
    };

    // ── Value node ──────────────────────────────────────────────────────
    // Represents the "payload" portion of a CLI command.
    // CLI parser determines the *kind*; Math parser interprets the math.

    struct ValueNode {
        enum Kind { RawExpr, SolveExpr, ExpandExpr, FactorExpr, SimplifyExpr };

        Kind        kind;
        std::string payload; // raw math text — untouched for math parser
    };

    // ── Flags ───────────────────────────────────────────────────────────

    enum class FlagType { Vars, Isolated, Fraction, Approx };

    // ── Command AST ─────────────────────────────────────────────────────

    struct CommandAST {
        CommandType                  type = CommandType::Unknown;
        std::string                  subcommand; // e.g. "list", "get" (config/env)
        std::vector<std::string>     args;       // positional args
        std::vector<FlagType>        flags;      // parsed flags
        std::vector<std::string>     flag_vars;  // values for -vars

        std::optional<ValueNode>     value;      // optional value expression

        // Original command text (for error messages / suggestions)
        std::string                  raw_command;
    };

} // namespace math_solver

#endif