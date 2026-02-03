#include "core/ast/expr.h"
#include "core/ast/equation.h"
#include "core/common/error.h"
#include "core/common/span.h"
#include "core/eval/context.h"
#include "core/eval/evaluator.h"
#include "core/parser/parser.h"
#include "core/solve/simplify.h"
#include "core/solve/solver.h"
#include <algorithm>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace std;
using namespace math_solver;

// ============================================================================
// Command Parsing Utilities
// ============================================================================

// Trim whitespace from both ends
string trim(const string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

// Split string by whitespace
vector<string> split(const string& s) {
    vector<string> tokens;
    istringstream iss(s);
    string token;
    while (iss >> token) {
        tokens.push_back(token);
    }
    return tokens;
}

// Check if string starts with prefix
bool starts_with(const string& s, const string& prefix) {
    return s.size() >= prefix.size() &&
           s.compare(0, prefix.size(), prefix) == 0;
}

// Parse flags from command (e.g., --vars, --isolated, --fraction)
struct CommandFlags {
    vector<string> vars;
    bool           isolated = false;
    bool           fraction = false;
    string         expression;  // Everything before flags
};

CommandFlags parse_flags(const string& input) {
    CommandFlags flags;

    // Find first flag
    size_t flag_start = input.find(" --");
    if (flag_start == string::npos) {
        flags.expression = input;
        return flags;
    }

    flags.expression = trim(input.substr(0, flag_start));

    // Parse flags
    string flag_part = input.substr(flag_start);
    vector<string> tokens = split(flag_part);

    for (size_t i = 0; i < tokens.size(); ++i) {
        if (tokens[i] == "--vars") {
            // Collect variable names until next flag or end
            while (i + 1 < tokens.size() && !starts_with(tokens[i + 1], "--")) {
                flags.vars.push_back(tokens[++i]);
            }
        } else if (tokens[i] == "--isolated") {
            flags.isolated = true;
        } else if (tokens[i] == "--fraction") {
            flags.fraction = true;
        }
    }

    return flags;
}

// ============================================================================
// Command Handlers
// ============================================================================

void print_help() {
    cout << "\n";
    cout << "Math Solver - Commands\n";
    cout << "======================\n\n";
    cout << "Evaluation:\n";
    cout << "  <expression>              Evaluate expression (e.g., 2 + 3 * 4)\n";
    cout << "  <expression> with vars    Use :set to define variables first\n\n";
    cout << "Variable Commands:\n";
    cout << "  :set <var> <value>        Set variable value (e.g., :set x 5)\n";
    cout << "  :unset <var>              Remove variable\n";
    cout << "  :clear                    Clear all variables\n";
    cout << "  :vars                     Show all defined variables\n\n";
    cout << "Equation Solving:\n";
    cout << "  solve <lhs> = <rhs>       Solve linear equation for unknown\n";
    cout << "                            (other vars substituted from context)\n\n";
    cout << "Simplification:\n";
    cout << "  simplify <lhs> = <rhs>    Simplify to canonical form Ax + By = C\n";
    cout << "  Options:\n";
    cout << "    --vars x y z            Specify variable order\n";
    cout << "    --isolated              Don't substitute from context\n";
    cout << "    --fraction              Display coefficients as fractions\n\n";
    cout << "Other:\n";
    cout << "  :help                     Show this help\n";
    cout << "  exit, quit                Exit the program\n\n";
    cout << "Examples:\n";
    cout << "  > 2 + 3 * 4\n";
    cout << "  = 14\n\n";
    cout << "  > :set y 4\n";
    cout << "  y = 4\n\n";
    cout << "  > solve 2*x + y = 10\n";
    cout << "  x = 3\n\n";
    cout << "  > simplify 12*x + 2 - 54*y * 3 = z --vars x y z\n";
    cout << "  12x - 162y - z = -2\n\n";
}

void cmd_set(const string& args, Context& ctx) {
    vector<string> parts = split(args);
    if (parts.size() < 2) {
        cout << "Usage: :set <variable> <value>\n";
        return;
    }

    string var_name = parts[0];

    // Check if valid identifier
    if (var_name.empty() ||
        !(isalpha(var_name[0]) || var_name[0] == '_')) {
        cout << "Error: invalid variable name '" << var_name << "'\n";
        return;
    }

    for (char c : var_name) {
        if (!isalnum(c) && c != '_') {
            cout << "Error: invalid variable name '" << var_name << "'\n";
            return;
        }
    }

    if (is_reserved_keyword(var_name)) {
        cout << "Error: '" << var_name << "' is a reserved keyword\n";
        return;
    }

    // Parse value (could be an expression)
    string value_str;
    for (size_t i = 1; i < parts.size(); ++i) {
        if (i > 1) value_str += " ";
        value_str += parts[i];
    }

    try {
        Parser parser(value_str);
        auto expr = parser.parse();
        Evaluator eval(&ctx, value_str);
        double value = eval.evaluate(*expr);
        ctx.set(var_name, value);

        // Format output nicely
        string val_str = to_string(value);
        size_t dot_pos = val_str.find('.');
        if (dot_pos != string::npos) {
            val_str.erase(val_str.find_last_not_of('0') + 1);
            if (val_str.back() == '.') val_str.pop_back();
        }
        cout << var_name << " = " << val_str << "\n";

    } catch (const MathError& e) {
        cout << e.format() << "\n";
    } catch (const exception& e) {
        cout << "Error: " << e.what() << "\n";
    }
}

void cmd_unset(const string& args, Context& ctx) {
    string var_name = trim(args);
    if (var_name.empty()) {
        cout << "Usage: :unset <variable>\n";
        return;
    }

    if (ctx.unset(var_name)) {
        cout << "Removed: " << var_name << "\n";
    } else {
        cout << "Variable '" << var_name << "' not found\n";
    }
}

void cmd_clear(Context& ctx) {
    size_t count = ctx.size();
    ctx.clear();
    cout << "Cleared " << count << " variable(s)\n";
}

void cmd_vars(const Context& ctx) {
    if (ctx.empty()) {
        cout << "No variables defined\n";
        return;
    }

    cout << "Variables:\n";
    for (const auto& pair : ctx.all()) {
        string val_str = to_string(pair.second);
        size_t dot_pos = val_str.find('.');
        if (dot_pos != string::npos) {
            val_str.erase(val_str.find_last_not_of('0') + 1);
            if (val_str.back() == '.') val_str.pop_back();
        }
        cout << "  " << pair.first << " = " << val_str << "\n";
    }
}

void cmd_solve(const string& args, Context& ctx) {
    if (args.empty()) {
        cout << "Usage: solve <lhs> = <rhs>\n";
        return;
    }

    try {
        Parser parser(args);
        auto eq = parser.parse_equation();

        EquationSolver solver(&ctx, args);
        SolveResult result = solver.solve(*eq);

        cout << result.to_string() << "\n";

    } catch (const MathError& e) {
        cout << e.format() << "\n";
        // Add hints for common issues
        if (dynamic_cast<const MultipleUnknownsError*>(&e)) {
            cout << "Hint: use :set to define variables, or use simplify\n";
        }
    } catch (const exception& e) {
        cout << "Error: " << e.what() << "\n";
    }
}

void cmd_simplify(const string& args, Context& ctx) {
    if (args.empty()) {
        cout << "Usage: simplify <lhs> = <rhs> [--vars x y] [--isolated] [--fraction]\n";
        return;
    }

    CommandFlags flags = parse_flags(args);

    if (flags.expression.empty()) {
        cout << "Usage: simplify <lhs> = <rhs> [--vars x y] [--isolated] [--fraction]\n";
        return;
    }

    try {
        Parser parser(flags.expression);
        auto eq = parser.parse_equation();

        SimplifyOptions opts;
        opts.var_order = flags.vars;
        opts.isolated = flags.isolated;
        opts.as_fraction = flags.fraction;

        Simplifier simplifier(&ctx, flags.expression);
        SimplifyResult result = simplifier.simplify(*eq, opts);

        // Print warnings
        for (const auto& warning : result.warnings) {
            cout << "Warning: " << warning << "\n";
        }

        // Print result with special cases
        if (result.is_no_solution()) {
            cout << result.canonical << "\n";
            cout << "=> no solution\n";
        } else if (result.is_infinite_solutions()) {
            cout << result.canonical << "\n";
            cout << "=> infinite solutions\n";
        } else {
            cout << result.canonical << "\n";
        }

    } catch (const MathError& e) {
        cout << e.format() << "\n";
    } catch (const exception& e) {
        cout << "Error: " << e.what() << "\n";
    }
}

void cmd_evaluate(const string& input, Context& ctx) {
    try {
        Parser parser(input);
        auto [expr, eq] = parser.parse_expression_or_equation();

        if (eq) {
            // It's an equation - evaluate both sides
            Evaluator eval(&ctx, input);
            double lhs_val = eval.evaluate(eq->lhs());
            double rhs_val = eval.evaluate(eq->rhs());

            // Format values
            auto format_val = [](double v) {
                string s = to_string(v);
                size_t dot = s.find('.');
                if (dot != string::npos) {
                    s.erase(s.find_last_not_of('0') + 1);
                    if (s.back() == '.') s.pop_back();
                }
                return s;
            };

            cout << format_val(lhs_val) << " = " << format_val(rhs_val);
            if (abs(lhs_val - rhs_val) < 1e-12) {
                cout << " (true)\n";
            } else {
                cout << " (false)\n";
            }
        } else {
            // Simple expression
            Evaluator eval(&ctx, input);
            double result = eval.evaluate(*expr);

            string result_str = to_string(result);
            size_t dot_pos = result_str.find('.');
            if (dot_pos != string::npos) {
                result_str.erase(result_str.find_last_not_of('0') + 1);
                if (result_str.back() == '.') result_str.pop_back();
            }
            cout << "= " << result_str << "\n";
        }

    } catch (const MathError& e) {
        cout << e.format() << "\n";
    } catch (const exception& e) {
        cout << "Error: " << e.what() << "\n";
    }
}

// ============================================================================
// Main Entry Point
// ============================================================================

int main(int argc, char* argv[]) {
    // Command-line mode: evaluate expression directly
    if (argc > 1) {
        string expr_str;
        for (int i = 1; i < argc; ++i) {
            if (i > 1) expr_str += " ";
            expr_str += argv[i];
        }

        Context ctx;  // Empty context for command-line mode

        try {
            Parser parser(expr_str);
            auto expr = parser.parse();
            Evaluator eval(&ctx, expr_str);
            cout << eval.evaluate(*expr) << endl;
        } catch (const MathError& e) {
            cerr << e.format() << endl;
            return 1;
        } catch (const exception& e) {
            cerr << "Error: " << e.what() << endl;
            return 1;
        }
        return 0;
    }

    // Interactive REPL mode
    cout << "Math Solver v1.0\n";
    cout << "Supports: expressions, variables, equations, solve, simplify\n";
    cout << "Type :help for commands, 'exit' to quit.\n\n";

    Context ctx;  // Shared persistent context

    string input;
    while (true) {
        cout << "> ";
        if (!getline(cin, input))
            break;

        input = trim(input);

        if (input.empty())
            continue;

        // Exit commands
        if (input == "exit" || input == "quit")
            break;

        // Help
        if (input == ":help" || input == "help") {
            print_help();
            continue;
        }

        // Variable commands
        if (starts_with(input, ":set ")) {
            cmd_set(input.substr(5), ctx);
            continue;
        }

        if (starts_with(input, ":unset ")) {
            cmd_unset(input.substr(7), ctx);
            continue;
        }

        if (input == ":clear") {
            cmd_clear(ctx);
            continue;
        }

        if (input == ":vars") {
            cmd_vars(ctx);
            continue;
        }

        // Solve command
        if (starts_with(input, "solve ")) {
            cmd_solve(input.substr(6), ctx);
            continue;
        }

        // Simplify command
        if (starts_with(input, "simplify ")) {
            cmd_simplify(input.substr(9), ctx);
            continue;
        }

        // Default: evaluate expression or equation
        cmd_evaluate(input, ctx);
    }

    return 0;
}