#pragma once

#include "ast/math/binary_expr.hpp"
#include "ast/math/equation_expr.hpp"
#include "ast/math/expr.hpp"
#include "ast/math/expr_visitor.hpp"
#include "ast/math/number_expr.hpp"
#include "ast/math/variable_expr.hpp"
#include "core/error.hpp"
#include "runtime/context/context.hpp"
#include <memory>
#include <string>
#include <unordered_set>

namespace math_solver {

    // Expander performs eager inlining of variable references using the
    // provided Context.
    // - Variable references are replaced with their definitions from Context,
    // if available.
    // - Cyclic variable definitions are detected and reported as errors (see
    // visited_).
    // - Expansion is non-caching: repeated expansion of the same variable along
    // different
    //   paths will re-traverse the subtree. This is intentional to avoid subtle
    //   caching bugs in the presence of context mutation or shadowing (though
    //   context is assumed immutable during expansion).
    // - Downstream passes must not assume variable references remain after
    // expansion.
    // - Equation nodes are intentionally skipped; handled by later lowering
    // passes.
    // - Correctness relies on Context being immutable for the duration of
    // expansion.
    // - visited_ tracks the expansion stack to ensure cycle detection is sound.
    class Expander : public ExprVisitor {
        private:
        ExprPtr                         result_;
        const Context&                  context_;
        std::string                     input_;
        std::unordered_set<std::string> visited_;

        public:
        explicit Expander(const Context& ctx) : context_(ctx), input_() {}

        Expander(const Context& ctx, const std::string& input)
            : context_(ctx), input_(input) {}

        // Entry point for expansion. visited_ is cleared to ensure no
        // cross-call contamination.
        ExprPtr expand(const Expr& expr) {
            visited_.clear();
            expr.accept(*this);
            return std::move(result_);
        }

        // Used for recursive expansion with explicit visited set (e.g., for
        // nested expansion).
        ExprPtr expand(const Expr&                      expr,
                       std::unordered_set<std::string>& visited) {
            visited_ = visited;
            expr.accept(*this);
            visited = visited_;
            return std::move(result_);
        }

        void visit(const Number& node) override { result_ = node.clone(); }

        void visit(const Variable& node) override {
            const std::string& name = node.name();
            // If variable is not present in Context, leave as-is.
            if (!context_.has(name)) {
                result_ = node.clone();
                return;
            }
            // Cycle detection: expansion stack is tracked in visited_.
            // This is required for soundness; otherwise, infinite recursion is
            // possible.
            if (visited_.count(name)) {
                throw CircularDependencyError(name, node.span(), input_);
            }
            visited_.insert(name);
            context_.get_expr(name).accept(*this);
            visited_.erase(name);
        }

        void visit(const BinaryOp& node) override {
            node.left().accept(*this);
            ExprPtr left = std::move(result_);
            node.right().accept(*this);
            ExprPtr right = std::move(result_);
            result_       = std::make_unique<BinaryOp>(
                std::move(left), std::move(right), node.op());
        }

        // Equation nodes are not expanded here; expansion is deferred to later
        // passes.
        void visit(const Equation& /*node*/) {}
    };

} // namespace math_solver
