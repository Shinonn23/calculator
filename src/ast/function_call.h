#ifndef FUNCTION_CALL_H
#define FUNCTION_CALL_H

#include "expr.h"
#include <memory>
#include <string>
#include <vector>

namespace math_solver {

    // Known built-in function names
    inline bool is_builtin_function(const std::string& name) {
        return name == "sqrt" || name == "abs" || name == "sin" ||
               name == "cos" || name == "tan" || name == "log" ||
               name == "ln" || name == "exp" || name == "floor" ||
               name == "ceil";
    }

    class FunctionCall : public Expr {
        private:
        std::string          name_;
        std::vector<ExprPtr> args_;

        public:
        FunctionCall(const std::string& name, std::vector<ExprPtr> args)
            : Expr(), name_(name), args_(std::move(args)) {}

        FunctionCall(const std::string& name, std::vector<ExprPtr> args,
                     const Span& span)
            : Expr(span), name_(name), args_(std::move(args)) {}

        // Convenience: single-argument function
        FunctionCall(const std::string& name, ExprPtr arg)
            : Expr(), name_(name) {
            args_.push_back(std::move(arg));
        }

        FunctionCall(const std::string& name, ExprPtr arg, const Span& span)
            : Expr(span), name_(name) {
            args_.push_back(std::move(arg));
        }

        const std::string&          name() const { return name_; }
        const std::vector<ExprPtr>& args() const { return args_; }
        size_t                      arg_count() const { return args_.size(); }

        const Expr&                 arg(size_t i) const { return *args_[i]; }

        void accept(ExprVisitor& visitor) const override {
            visitor.visit(*this);
        }

        std::string to_string() const override {
            std::string result = name_ + "(";
            for (size_t i = 0; i < args_.size(); ++i) {
                if (i > 0)
                    result += ", ";
                result += args_[i]->to_string();
            }
            result += ")";
            return result;
        }

        std::unique_ptr<Expr> clone() const override {
            std::vector<ExprPtr> cloned_args;
            cloned_args.reserve(args_.size());
            for (const auto& a : args_) {
                cloned_args.push_back(a->clone());
            }
            auto cloned =
                std::make_unique<FunctionCall>(name_, std::move(cloned_args));
            cloned->set_span(span_);
            return cloned;
        }
    };

} // namespace math_solver

#endif
