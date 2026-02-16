#ifndef EXPAND_H
#define EXPAND_H

#include "../common/color.hpp"
#include "../common/error.hpp"
#include "../parser/parser.hpp"
#include "ast_to_poly.hpp"
#include "polynomial.hpp"
#include <iostream>
#include <string>

namespace math_solver {

    inline void cmd_expand(const std::string& args) {
        if (args.empty()) {
            std::cout << "  Usage: expand <expression>\n";
            return;
        }

        try {
            Parser          parser(args);
            auto            expr = parser.parse();

            ASTToPolynomial converter(args);
            Polynomial      poly = converter.convert(*expr);

            std::cout << "  " << poly.to_string() << "\n";
        } catch (const MathError& e) {
            std::cout << e.format() << "\n";
        } catch (const std::exception& e) {
            std::cout << ansi::red << "  Error: " << ansi::reset << e.what()
                      << "\n";
        }
    }

} // namespace math_solver

#endif
