#ifndef EXPANDER_H
#define EXPANDER_H

#include "ast/binary.hpp"
#include "ast/expr.hpp"
#include "ast/number.hpp"
#include "ast/variable.hpp"
#include "common/error.hpp"
#include "context.hpp"
#include <memory>
#include <string>
#include <unordered_set>

namespace math_solver {

    // Recursively expands variable references in an expression
    // by substituting their stored expressions from the context.
    // Used for symbolic display (e.g., showing "3(x + 2)" for b = a*3, a = x+2).
    class Expander : public ExprVisitor {
        private:
        ExprPtr                         result_;
        const Context&                  context_;
        std::unordered_set<std::string> visited_;

        public:
        explicit Expander(const Context& ctx) : context_(ctx) {}

        // Expand all variable references in an expression.
        // Variables not in context are kept as-is.
        // Throws CircularDependencyError if a cycle is detected.
        ExprPtr expand(const Expr& expr) {
            visited_.clear();
            expr.accept(*this);
            return std::move(result_);
        }

        // Expand with an initial visited set (for nested calls)
        ExprPtr expand(const Expr& expr,
                       std::unordered_set<std::string>& visited) {
            visited_ = visited;
            expr.accept(*this);
            visited = visited_;
            return std::move(result_);
        }

        void visit(const Number& node) override {
            result_ = node.clone();
        }

        void visit(const Variable& node) override {
            const std::string& name = node.name();

            // If not in context, keep as variable
            if (!context_.has(name)) {
                result_ = node.clone();
                return;
            }

            // Cycle detection
            if (visited_.count(name)) {
                throw CircularDependencyError(name, node.span());
            }

            // Recursively expand the stored expression
            visited_.insert(name);
            const Expr& stored = context_.get_expr(name);
            stored.accept(*this);
            visited_.erase(name);
        }

        void visit(const BinaryOp& node) override {
            // Expand both children
            node.left().accept(*this);
            ExprPtr left = std::move(result_);

            node.right().accept(*this);
            ExprPtr right = std::move(result_);

            result_ = std::make_unique<BinaryOp>(
                std::move(left), std::move(right), node.op());
        }
    };

} // namespace math_solver

#endif
