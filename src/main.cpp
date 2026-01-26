#include "core/ast/expr.h"
#include "core/eval/evaluator.h"
#include "core/parser/parser.h"
#include <iostream>
#include <string>

using namespace std;
using namespace math_solver;

int main(int argc, char* argv[]) {
    // Command-line mode: evaluate expression directly
    if (argc > 1) {
        string expr_str;
        for (int i = 1; i < argc; ++i) {
            if (i > 1)
                expr_str += " ";
            expr_str += argv[i];
        }

        try {
            Parser    parser(expr_str);
            auto      expr = parser.parse();
            Evaluator eval;
            cout << eval.evaluate(*expr) << endl;
        } catch (const exception& e) {
            cerr << "Error: " << e.what() << endl;
            return 1;
        }
        return 0;
    }

    // Interactive REPL mode
    cout << "Math Solver - Simple Calculator\n";
    cout << "Supports: + - * / ^ ()\n";
    cout << "Type 'exit' to quit.\n\n";

    string input;
    while (true) {
        cout << "> ";
        if (!getline(cin, input))
            break;

        // Trim whitespace
        size_t start = input.find_first_not_of(" \\t");
        if (start == string::npos)
            continue;
        input = input.substr(start);

        if (input.empty())
            continue;
        if (input == "exit" || input == "quit")
            break;

        try {
            Parser    parser(input);
            auto      expr = parser.parse();
            Evaluator eval;
            cout << "= " << eval.evaluate(*expr) << endl;
        } catch (const exception& e) {
            cout << "Error: " << e.what() << endl;
        }
    }

    return 0;
}