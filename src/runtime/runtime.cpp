#include "runtime.hpp"
#include "ui/color.hpp"
#include "ui/suggestions.hpp"
#include "parser/math/math_parser.hpp" // สมมติว่าไฟล์ Parser อยู่ตรงนี้
#include <iostream>

using namespace std;

namespace math_solver {

    void Runtime::save_environment() {
        config_.save_env_variables(current_env_, context_.all_as_strings());
    }

    bool Runtime::load_environment(const std::string& env_name) {
        if (!config_.env_exists(env_name)) {
            cout << ansi::red << "  Error: " << ansi::reset << "environment '"
                 << env_name << "' not found\n";
            auto envs  = config_.list_envs();
            auto match = suggest(env_name, envs);
            if (match) {
                cout << ansi::dim << "  Did you mean " << ansi::reset
                     << ansi::bold << *match << ansi::reset << ansi::dim << "?"
                     << ansi::reset << "\n";
            }
            return false; // โหลดไม่สำเร็จ
        }

        // เซฟของเก่าก่อนเปลี่ยน
        save_environment();

        context_.clear();
        const auto& env = config_.get_env(env_name);
        
        for (const auto& [name, expr_str] : env.variables) {
            try {
                Parser parser(expr_str);
                auto expr = parser.parse();
                context_.set(name, std::move(expr));
            } catch (...) {
                try {
                    double val = std::stod(expr_str);
                    context_.set(name, val);
                } catch (...) {
                    // ข้ามตัวที่พัง
                }
            }
        }
        current_env_ = env_name;
        return true;
    }

    double Runtime::evaluate(const std::string& var_name) const {
        return Resolver::evaluate_variable(var_name, context_);
    }

} // namespace math_solver