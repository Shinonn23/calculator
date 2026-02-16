#ifndef CLI_PARSER_H
#define CLI_PARSER_H

#include "command_ast.hpp"
#include <string>

namespace math_solver {

    // Strip `-- ...` comments from end of line.
    std::string strip_comment(const std::string& input);

    // Parse any input line into a CommandAST.
    // - ":" prefix   → CLI command (Set, Solve, Config, etc.)
    // - "exit/quit"  → Exit
    // - bare input   → MathExpr (payload in value.payload)
    CommandAST parse_cli(const std::string& input);

} // namespace math_solver

#endif
