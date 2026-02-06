#ifndef CONSOLE_UI_H
#define CONSOLE_UI_H

#include "common/error.h"
#include "common/format_utils.h"
#include "common/fraction.h"
#include "eval/context.h"
#include "solve/linear_system.h"
#include "solve/simplify.h"
#include "solve/solver.h"

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

namespace math_solver {

    class ConsoleUI {
        private:
        std::ostream&           out_;
        std::ostream&           err_;

        static constexpr size_t MAX_PROMPT_VARS = 4;

        public:
        ConsoleUI(std::ostream& out, std::ostream& err)
            : out_(out), err_(err) {}

        // ─── Prompt ───────────────────────────────────────────────

        // Generate context-aware prompt: "math> " or "math[x,y]> "
        std::string prompt(const Context& ctx) const {
            if (ctx.empty()) {
                return "math> ";
            }

            // Gather and sort variable names
            auto names = ctx.all_names();
            std::sort(names.begin(), names.end());

            std::string result = "math[";
            size_t      show   = std::min(names.size(), MAX_PROMPT_VARS);
            for (size_t i = 0; i < show; ++i) {
                if (i > 0)
                    result += ",";
                result += names[i];
            }
            if (names.size() > MAX_PROMPT_VARS) {
                result += ",+" + std::to_string(names.size() - MAX_PROMPT_VARS);
            }
            result += "]> ";
            return result;
        }

        // Sub-prompt for multi-equation mode: "system[1]> "
        std::string system_prompt(int eq_num) const {
            return "system[" + std::to_string(eq_num) + "]> ";
        }

        // ─── Banner & File Headers ───────────────────────────────

        void print_banner() {
            out_ << "Math Solver v1.0\n";
            out_ << "Type 'help' for commands, 'exit' to quit\n\n";
        }

        void print_file_header(const std::string& filename) {
            out_ << "[Running " << filename << "]\n";
        }

        void print_file_footer(const std::string& filename, bool stopped) {
            if (stopped) {
                err_ << "[Execution stopped]\n";
            } else {
                out_ << "[Finished " << filename << "]\n";
            }
        }

        // ─── Result Printers ─────────────────────────────────────

        void print_set(const std::string& var, double value) {
            out_ << var << " = " << format_double(value) << "\n";
        }

        void print_unset(const std::string& var) {
            out_ << "Removed: " << var << "\n";
        }

        void print_unset_not_found(const std::string& var) {
            err_ << "Variable '" << var << "' not found\n";
        }

        void print_clear(size_t count) {
            out_ << "Cleared " << count << " variable(s)\n";
        }

        void print_vars(const Context& ctx) {
            if (ctx.empty()) {
                out_ << "No variables defined.\n";
                return;
            }

            // Sort variable names for consistent output
            auto names = ctx.all_names();
            std::sort(names.begin(), names.end());

            out_ << "Variables:\n";
            for (const auto& name : names) {
                out_ << "  " << name << " = "
                     << format_double(ctx.all().at(name)) << "\n";
            }
        }

        void print_eval_expr(double value) {
            out_ << format_double(value) << "\n";
        }

        void print_eval_equation(double lhs, double rhs, bool equal) {
            out_ << format_double(lhs) << " = " << format_double(rhs);
            if (equal) {
                out_ << " (true)\n";
            } else {
                out_ << " (false)\n";
            }
        }

        void print_solve(const SolveResult& result) {
            if (!result.has_solution) {
                out_ << "No solution\n";
                return;
            }
            out_ << "Solution:\n";
            out_ << "  " << result.variable << " = "
                 << format_double(result.value) << "\n";
        }

        void print_solve_system_info(size_t num_eq, size_t num_vars) {
            out_ << "\nSystem: " << num_eq << " equation(s), " << num_vars
                 << " variable(s)\n";
        }

        void print_solve_system(const SystemSolution& result,
                                bool                  as_fraction) {
            switch (result.type) {
            case SolutionType::NoSolution:
                out_ << "\nNo solution (inconsistent system)\n";
                break;

            case SolutionType::Infinite:
                out_ << "\nInfinite solutions\n";
                if (!result.free_variables.empty()) {
                    out_ << "Free variables: ";
                    for (size_t i = 0; i < result.free_variables.size(); ++i) {
                        if (i > 0)
                            out_ << ", ";
                        out_ << result.free_variables[i];
                    }
                    out_ << "\n";
                }
                break;

            case SolutionType::Unique:
                out_ << "\nSolution:\n";
                for (size_t i = 0; i < result.variables.size(); ++i) {
                    out_ << "  " << result.variables[i] << " = ";
                    if (as_fraction) {
                        Fraction frac = double_to_fraction(result.values[i]);
                        out_ << frac.to_string();
                    } else {
                        out_ << format_double(result.values[i]);
                    }
                    out_ << "\n";
                }
                break;
            }
        }

        void print_simplify(const SimplifyResult& result) {
            // Print warnings first
            for (const auto& warning : result.warnings) {
                err_ << "Warning: " << warning << "\n";
            }

            if (result.is_no_solution()) {
                out_ << "Canonical form:\n";
                out_ << "  " << result.canonical << "\n";
                out_ << "  => no solution\n";
            } else if (result.is_infinite_solutions()) {
                out_ << "Canonical form:\n";
                out_ << "  " << result.canonical << "\n";
                out_ << "  => infinite solutions\n";
            } else {
                out_ << "Canonical form:\n";
                out_ << "  " << result.canonical << "\n";
            }
        }

        // ─── Error & Info ────────────────────────────────────────

        void print_error(const MathError& e) { err_ << e.format() << "\n"; }

        void print_error(const std::string& message) {
            err_ << "Error: " << message << "\n";
        }

        void print_hint(const std::string& message) {
            out_ << "Hint: " << message << "\n";
        }

        void print_info(const std::string& message) { out_ << message << "\n"; }

        void print_warning(const std::string& message) {
            err_ << "Warning: " << message << "\n";
        }

        void print_usage(const std::string& message) {
            out_ << "Usage: " << message << "\n";
        }

        // ─── Help System ─────────────────────────────────────────

        void print_help(const std::string& topic = "") {
            if (topic.empty()) {
                print_help_overview();
            } else if (topic == "set") {
                print_help_set();
            } else if (topic == "unset") {
                print_help_unset();
            } else if (topic == "vars") {
                print_help_vars();
            } else if (topic == "clear") {
                print_help_clear();
            } else if (topic == "solve") {
                print_help_solve();
            } else if (topic == "simplify") {
                print_help_simplify();
            } else {
                err_ << "Unknown help topic: '" << topic << "'\n";
                out_ << "Type 'help' for available commands.\n";
            }
        }

        private:
        void print_help_overview() {
            out_ << "\n";
            out_ << "Math Solver - Commands\n";
            out_ << "======================\n\n";
            out_ << "  Evaluation:\n";
            out_ << "    <expression>              Evaluate (e.g., 2 + 3 * "
                    "4)\n\n";
            out_ << "  Variables:\n";
            out_ << "    set <var> <value>          Set variable\n";
            out_ << "    unset <var>                Remove variable\n";
            out_ << "    clear                      Clear all variables\n";
            out_ << "    vars                       Show all variables\n\n";
            out_ << "  Solving:\n";
            out_ << "    solve <lhs> = <rhs>        Solve single equation\n";
            out_ << "    solve                      Multi-equation mode\n\n";
            out_ << "  Simplification:\n";
            out_ << "    simplify <lhs> = <rhs>     Simplify to canonical "
                    "form\n\n";
            out_ << "  Other:\n";
            out_ << "    help [command]              Show help\n";
            out_ << "    exit, q                    Quit\n\n";
            out_ << "Type 'help <command>' for details (e.g., help solve)\n\n";
        }

        void print_help_set() {
            out_ << "\nSet variable\n\n";
            out_ << "Usage:\n";
            out_ << "  set <variable> <expression>\n\n";
            out_ << "Examples:\n";
            out_ << "  set x 5\n";
            out_ << "  set y 2*x + 3\n";
            out_ << "  set area 3.14159*r*r\n\n";
        }

        void print_help_unset() {
            out_ << "\nRemove variable\n\n";
            out_ << "Usage:\n";
            out_ << "  unset <variable>\n\n";
            out_ << "Example:\n";
            out_ << "  unset x\n\n";
        }

        void print_help_vars() {
            out_ << "\nShow all defined variables\n\n";
            out_ << "Usage:\n";
            out_ << "  vars\n\n";
        }

        void print_help_clear() {
            out_ << "\nClear all variables\n\n";
            out_ << "Usage:\n";
            out_ << "  clear\n\n";
        }

        void print_help_solve() {
            out_ << "\nSolve equations\n\n";
            out_ << "Usage:\n";
            out_ << "  solve <lhs> = <rhs>       Solve single equation\n";
            out_ << "  solve                     Multi-equation mode:\n";
            out_
                << "                            Enter equations one per line\n";
            out_ << "                            Empty line to solve\n\n";
            out_ << "Options:\n";
            out_ << "  --vars x y z              Specify variable order\n";
            out_ << "  --fraction                Display results as "
                    "fractions\n\n";
            out_ << "Examples:\n";
            out_ << "  solve 2x + 4 = 0\n";
            out_ << "  solve x + y = 10          (multi-equation mode)\n\n";
        }

        void print_help_simplify() {
            out_ << "\nSimplify to canonical form\n\n";
            out_ << "Usage:\n";
            out_ << "  simplify <lhs> = <rhs>\n\n";
            out_ << "Options:\n";
            out_ << "  --vars x y z              Specify variable order\n";
            out_ << "  --isolated                Don't substitute from "
                    "context\n";
            out_ << "  --fraction                Display as fractions\n\n";
            out_ << "Example:\n";
            out_ << "  simplify 4x + 8y = 16\n";
            out_ << "  simplify 4x + 8y = 16 --fraction\n\n";
        }
    };

} // namespace math_solver

#endif
