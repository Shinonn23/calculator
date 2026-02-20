#ifndef MATH_SOLVER_RUNTIME_HPP
#define MATH_SOLVER_RUNTIME_HPP

#include "context/context.hpp"
#include "context/resolver.hpp"
#include "config/config.hpp"
#include <string>

namespace math_solver {

    // คลาสนี้มัดรวมการทำงานระหว่าง State กับการประมวลผล
    class Runtime {
    private:
        Context context_;
        Config& config_;          // อ้างอิงไปยัง Config หลัก
        std::string current_env_;

    public:
        Runtime(Config& config, const std::string& default_env = "default") 
            : config_(config), current_env_(default_env) {}

        // เข้าถึง Context ปัจจุบัน
        Context& get_context() { return context_; }
        const Context& get_context() const { return context_; }

        // เข้าถึงข้อมูล Environment
        const std::string& current_env() const { return current_env_; }

        // บันทึกตัวแปรลง Config
        void save_environment();

        // โหลดตัวแปรจาก Config ขึ้นมาใน Context
        bool load_environment(const std::string& env_name);

        // หาผลลัพธ์ของตัวแปร (Wrapper หุ้ม Resolver)
        double evaluate(const std::string& var_name) const;
    };

} // namespace math_solver

#endif // MATH_SOLVER_RUNTIME_HPP