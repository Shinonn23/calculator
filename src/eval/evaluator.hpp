#pragma once

#include "ast/math/array_expr.hpp"
#include "ast/math/binary_expr.hpp"
#include "ast/math/expr_visitor.hpp"
#include "ast/math/number_expr.hpp"
#include "ast/math/unary_expr.hpp"
#include "ast/math/variable_expr.hpp"
#include "diagnostics/sink.hpp"
#include "runtime/context/context.hpp"

namespace math_solver {

    // Evaluator is responsible for traversing the AST and computing the result
    // of an expression.
    //
    // - The evaluation is stateless except for `visited_`, which is used to
    // detect cycles in variable references.
    // - `context_` provides variable bindings; must remain valid for the
    // lifetime of the Evaluator.
    // - `input_` is used for diagnostics or error reporting, not for evaluation
    // logic.
    // - Assumes the AST is well-formed and does not mutate it.
    // - Not thread-safe; intended for single-threaded use.
    //
    // Performance: The use of `unordered_set` for `visited_` is O(1) per
    // insertion, but may be a bottleneck
    //   if evaluating deeply recursive or cyclic expressions. Consider
    //   alternatives if performance becomes critical.
    //
    // Invariants:
    // - `result_` is only valid after a call to `evaluate`.
    // - `visited_` is cleared at the start of each evaluation.
    // - No side effects on `context_` or the AST.
    class Evaluator : public ExprVisitor {
        private:
        double                                result_;
        const Context*                        context_;
        std::string                           input_;
        DiagnosticSink*                       sink_;
        std::unordered_map<std::string, Span> visited_;

        public:
        Evaluator()
            : result_(0.0), context_(nullptr), input_(), sink_(nullptr) {}

        explicit Evaluator(const Context* ctx, DiagnosticSink* sink = nullptr)
            : result_(0.0), context_(ctx), input_(), sink_(sink) {}

        Evaluator(const Context* ctx, const std::string& input,
                  DiagnosticSink* sink = nullptr)
            : result_(0.0), context_(ctx), input_(input), sink_(sink) {}

        void   set_input(const std::string& input) { input_ = input; }

        // Entry point for evaluation. The caller must ensure that `expr` is
        // valid for the duration of the call. Returns the computed value. Side
        // effects: resets `visited_` and updates `result_`.
        double evaluate(const Expr& expr) {
            visited_.clear();
            expr.accept(*this);
            return result_;
        }

        // The following visit methods implement the core evaluation logic for
        // each AST node type. They must update `result_` with the computed
        // value.
        void visit(const Number& node) override;
        void visit(const BinaryOp& node) override;
        void visit(const UnaryOp& node) override;
        void visit(const Variable& node) override;
        // ArrayExpr in scalar context emits an error via sink.
        void visit(const ArrayExpr& node) override;

        // Broadcast evaluation: resolves array-bound variables in expr and
        // evaluates the expression for each element, returning the vector of
        // results. Emits to sink on size-mismatch or other error.
        std::vector<double> evaluate_broadcast(const Expr&    expr,
                                               const Context& ctx);
        // Equation nodes are not handled here; see solver logic for details.
    };

} // namespace math_solver
