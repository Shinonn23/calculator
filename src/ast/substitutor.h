#ifndef SUBSTITUTOR_H
#define SUBSTITUTOR_H

#include "binary.h"
#include "expr.h"
#include "function_call.h"
#include "index_access.h"
#include "number.h"
#include "number_array.h"
#include "variable.h"

#include <functional>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace math_solver {

    // Forward declaration — Context stores ExprPtr, substitutor needs to
    // look up expressions by variable name. We use a callback to avoid
    // circular dependency with context.h.
    using ExprLookupFn = std::function<const Expr*(const std::string&)>;

    // Visitor that substitutes variables with their stored expressions.
    // Produces a new expression tree with all known variables expanded.
    class SubstitutionVisitor : public ExprVisitor {
        private:
        ExprPtr               result_;
        ExprLookupFn          lookup_;
        int                   depth_;
        int                   max_depth_;
        std::set<std::string> expanding_; // cycle detection

        public:
        explicit SubstitutionVisitor(ExprLookupFn lookup, int max_depth = 100)
            : lookup_(std::move(lookup)), depth_(0), max_depth_(max_depth) {}

        // Expand all known variables in the expression, returning a new tree.
        ExprPtr expand(const Expr& expr) {
            if (depth_ >= max_depth_) {
                throw std::runtime_error("maximum expansion depth exceeded "
                                         "(possible circular reference)");
            }
            ++depth_;
            expr.accept(*this);
            --depth_;
            return std::move(result_);
        }

        void visit(const Number& node) override { result_ = node.clone(); }

        void visit(const Variable& node) override {
            const std::string& name   = node.name();
            const Expr*        stored = lookup_(name);

            if (stored) {
                // Cycle detection
                if (expanding_.count(name)) {
                    throw std::runtime_error(
                        "circular variable reference detected involving '" +
                        name + "'");
                }
                expanding_.insert(name);
                result_ = expand(*stored);
                expanding_.erase(name);
            } else {
                // Unknown variable — keep as-is
                result_ = node.clone();
            }
        }

        void visit(const BinaryOp& node) override {
            ExprPtr left     = expand(node.left());
            ExprPtr right    = expand(node.right());
            auto    expanded = std::make_unique<BinaryOp>(
                std::move(left), std::move(right), node.op());
            expanded->set_span(node.span());
            result_ = std::move(expanded);
        }

        void visit(const FunctionCall& node) override {
            std::vector<ExprPtr> expanded_args;
            expanded_args.reserve(node.arg_count());
            for (size_t i = 0; i < node.arg_count(); ++i) {
                expanded_args.push_back(expand(node.arg(i)));
            }
            auto expanded = std::make_unique<FunctionCall>(
                node.name(), std::move(expanded_args));
            expanded->set_span(node.span());
            result_ = std::move(expanded);
        }

        void visit(const NumberArray& node) override { result_ = node.clone(); }

        void visit(const IndexAccess& node) override {
            ExprPtr expanded_target = expand(node.target());
            // If the expanded target is a NumberArray, resolve the index now
            auto* arr = dynamic_cast<const NumberArray*>(expanded_target.get());
            if (arr) {
                if (node.index() < arr->size()) {
                    result_ = std::make_unique<Number>(arr->at(node.index()));
                } else {
                    throw std::runtime_error(
                        "index " + std::to_string(node.index()) +
                        " out of range (array has " +
                        std::to_string(arr->size()) + " elements)");
                }
            } else {
                auto rebuilt = std::make_unique<IndexAccess>(
                    std::move(expanded_target), node.index());
                rebuilt->set_span(node.span());
                result_ = std::move(rebuilt);
            }
        }
    };

    // Convenience: expand an expression given a lookup function.
    inline ExprPtr expand_expr(const Expr& expr, ExprLookupFn lookup) {
        SubstitutionVisitor visitor(std::move(lookup));
        return visitor.expand(expr);
    }

    // Collect all free (unresolved) variable names in an expression.
    class FreeVarCollector : public ExprVisitor {
        private:
        std::set<std::string> vars_;

        public:
        std::set<std::string> collect(const Expr& expr) {
            vars_.clear();
            expr.accept(*this);
            return vars_;
        }

        void visit(const Number&) override {}

        void visit(const Variable& node) override { vars_.insert(node.name()); }

        void visit(const BinaryOp& node) override {
            node.left().accept(*this);
            node.right().accept(*this);
        }

        void visit(const FunctionCall& node) override {
            for (size_t i = 0; i < node.arg_count(); ++i) {
                node.arg(i).accept(*this);
            }
        }

        void visit(const NumberArray&) override {}

        void visit(const IndexAccess& node) override {
            node.target().accept(*this);
        }
    };

    // Collect all free variables in an expression.
    inline std::set<std::string> free_variables(const Expr& expr) {
        FreeVarCollector collector;
        return collector.collect(expr);
    }

} // namespace math_solver

#endif
