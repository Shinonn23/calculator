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

    // Expander is responsible for recursively inlining variable references
    // within an expression tree, using the current Context as the source of
    // variable bindings. This is a pre-pass that canonicalizes expressions
    // before further lowering or evaluation.
    //
    // Invariants:
    // - Variable references are replaced with their definitions from Context,
    //   unless the variable is not present in Context.
    // - Circular dependencies are detected and reported as errors.
    //
    // Subtlety:
    // - The visited_ set is used to track the expansion stack and prevent
    //   infinite recursion on cycles. This is critical for soundness.
    // - The expansion is performed eagerly; downstream passes must not assume
    //   that variable references remain unexpanded.
    //
    // Performance:
    // - Each variable is expanded at most once per expansion path. No caching
    //   is performed; repeated expansions may be costly if expressions are
    //   large.
    //
    // Interactions:
    // - Assumes Context is immutable during expansion.
    // - Equation nodes are not handled here; see downstream passes.
    class Expander : public ExprVisitor {
        private:
        ExprPtr                         result_;
        const Context&                  context_;
        std::unordered_set<std::string> visited_;

        public:
        explicit Expander(const Context& ctx) : context_(ctx) {}

        ExprPtr expand(const Expr& expr) {
            visited_.clear();
            expr.accept(*this);
            return std::move(result_);
        }

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
            // If the variable is not bound in Context, leave as-is.
            if (!context_.has(name)) {
                result_ = node.clone();
                return;
            }
            // Detect cycles in variable expansion. This is required for
            // soundness.
            if (visited_.count(name)) {
                throw CircularDependencyError(name, node.span());
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

        void visit(const Equation& /*node*/) {
        } // Not handled here; see downstream passes.
    };

} // namespace math_solver
