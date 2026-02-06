#ifndef EVALUATOR_H
#define EVALUATOR_H

#include "ast/expr.h"
#include "ast/function_call.h"
#include "ast/index_access.h"
#include "ast/number_array.h"
#include "common/value.h"
#include "context.h"

#include <set>
#include <string>

namespace math_solver {

    class Evaluator : public ExprVisitor {
        private:
        Value                 result_;
        const Context*        context_;
        std::string           input_; // For error formatting
        EvaluationConfig      config_;

        // Guard against circular evaluation
        std::set<std::string> evaluating_;

        // RAII guard for evaluating_ set — ensures cleanup on throw
        struct EvalGuard {
            std::set<std::string>& set;
            std::string            name;

            EvalGuard(std::set<std::string>& s, const std::string& n)
                : set(s), name(n) {
                set.insert(name);
            }
            ~EvalGuard() { set.erase(name); }

            EvalGuard(const EvalGuard&)            = delete;
            EvalGuard& operator=(const EvalGuard&) = delete;
        };

        // ── Internal vector helpers ─────────────────────────────
        static Value apply_binary(const Value& left, const Value& right,
                                  BinaryOpType op, const Span& span,
                                  const std::string& input);

        static Value apply_func(const std::string& name, const Value& arg,
                                const Span& span, const std::string& input);

        public:
        Evaluator() : result_(), context_(nullptr), input_(), config_() {}

        explicit Evaluator(const Context* ctx)
            : result_(), context_(ctx), input_(), config_() {}

        Evaluator(const Context* ctx, const std::string& input)
            : result_(), context_(ctx), input_(input), config_() {}

        Evaluator(const Context* ctx, const std::string& input,
                  const EvaluationConfig& config)
            : result_(), context_(ctx), input_(input), config_(config) {}

        void  set_input(const std::string& input) { input_ = input; }

        void  set_config(const EvaluationConfig& config) { config_ = config; }

        // ── Primary API ─────────────────────────────────────────
        // Returns a Value (scalar or vector).
        Value evaluate(const Expr& expr) {
            expr.accept(*this);
            return result_;
        }

        // Convenience: evaluate and extract scalar.
        // Throws if result is a multi-element vector.
        double evaluate_scalar(const Expr& expr) {
            expr.accept(*this);
            return result_.as_scalar();
        }

        void visit(const Number& node) override;
        void visit(const BinaryOp& node) override;
        void visit(const Variable& node) override;
        void visit(const FunctionCall& node) override;
        void visit(const NumberArray& node) override;
        void visit(const IndexAccess& node) override;
    };

} // namespace math_solver

#endif
