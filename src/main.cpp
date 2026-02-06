#include "ast/equation.h"
#include "ast/expr.h"
#include "common/command.h"
#include "common/console_ui.h"
#include "common/error.h"
#include "common/utils.h"
#include "eval/evaluator.h"
#include "parser/parser.h"
#include "solve/linear_system.h"
#include "solve/simplify.h"
#include "solve/solver.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace std;
using namespace math_solver;

int main(int argc, char* argv[]) {
    string         file_path;
    bool           file_mode = false;
    vector<string> expr_args;

    for (int i = 1; i < argc; ++i) {
        string arg = argv[i];
        if (arg == "--file" || arg == "-f") {
            if (file_mode) {
                cerr << "Error: --file specified multiple times\n";
                return 1;
            }
            if (i + 1 >= argc) {
                cerr << "Error: missing path for --file\n";
                return 1;
            }
            file_mode = true;
            file_path = argv[++i];
        } else {
            expr_args.push_back(arg);
        }
    }

    if (!file_mode && expr_args.size() == 1 && ends_with(expr_args[0], ".ms")) {
        file_mode = true;
        file_path = expr_args[0];
        expr_args.clear();
    }

    if (file_mode) {
        if (!ends_with(file_path, ".ms")) {
            cerr << "Error: file must have .ms extension\n";
            return 1;
        }
        if (!expr_args.empty()) {
            cerr << "Error: cannot combine --file with expression arguments\n";
            return 1;
        }

        ifstream file(file_path);
        if (!file) {
            cerr << "Error: cannot open file '" << file_path << "'\n";
            return 1;
        }

        ConsoleUI ui(cout, cerr);
        Context   ctx;
        string    line;

        ui.print_file_header(file_path);

        while (getline(file, line)) {
            if (!process_input_line(line, ctx, file, false, ui)) {
                break;
            }
        }

        ui.print_file_footer(file_path, false);
        return 0;
    }

    if (!expr_args.empty()) {
        string expr_str;
        for (size_t i = 0; i < expr_args.size(); ++i) {
            if (i > 0)
                expr_str += " ";
            expr_str += expr_args[i];
        }

        Context ctx; // Empty context for command-line mode

        try {
            Parser    parser(expr_str);
            auto      expr = parser.parse();
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

    // REPL mode
    ConsoleUI ui(cout, cerr);
    ui.print_banner();

    Context ctx;
    string  input;

    while (true) {
        cout << ui.prompt(ctx);
        if (!getline(cin, input))
            break;

        if (!process_input_line(input, ctx, cin, true, ui))
            break;
    }

    return 0;
}
