#ifndef DOMAIN_H
#define DOMAIN_H

#include "ast/binary.h"
#include "ast/expr.h"
#include "ast/function_call.h"
#include "ast/index_access.h"
#include "ast/number.h"
#include "ast/number_array.h"
#include "ast/variable.h"
#include "eval/context.h"
#include "eval/evaluator.h"
#include <cmath>
#include <memory>
#include <string>
#include <vector>

namespace math_solver {

    // ---------------------------------------------------------------
    // DomainConstraint: a single restriction on the solution variable.
    //   kind       – what generated it (div, sqrt, log, …)
    //   expr       – the subexpression that must satisfy the constraint
    //   description – human-readable explanation
    // ---------------------------------------------------------------
    enum class ConstraintKind { DivByZero, SqrtNeg, LogNonPos };

    struct DomainConstraint {
        ConstraintKind kind;
        const Expr*    expr; // borrowed ptr – lives as long as AST
        std::string    description;
    };

    // ---------------------------------------------------------------
    // DomainCollector: AST visitor that gathers all domain constraints
    // from an expression tree.  Non-owning – the constraints reference
    // nodes inside the visited expression, which must stay alive.
    // ---------------------------------------------------------------
    class DomainCollector : public ExprVisitor {
        private:
        std::vector<DomainConstraint> constraints_;

        public:
        const std::vector<DomainConstraint>& constraints() const {
            return constraints_;
        }

        void collect(const Expr& expr) {
            constraints_.clear();
            expr.accept(*this);
        }

        // -- visitor impl ------------------------------------------------

        void visit(const Number&) override {}
        void visit(const Variable&) override {}
        void visit(const NumberArray&) override {}

        void visit(const IndexAccess& node) override {
            node.target().accept(*this);
        }

        void visit(const BinaryOp& node) override {
            // Recurse into children first
            node.left().accept(*this);
            node.right().accept(*this);

            // Division: denominator must not be zero
            if (node.op() == BinaryOpType::Div) {
                constraints_.push_back(
                    {ConstraintKind::DivByZero, &node.right(),
                     "denominator " + node.right().to_string() + " != 0"});
            }
        }

        void visit(const FunctionCall& node) override {
            // Recurse into arguments
            for (size_t i = 0; i < node.arg_count(); ++i)
                node.arg(i).accept(*this);

            if (node.arg_count() != 1)
                return;

            const Expr& arg = node.arg(0);
            if (node.name() == "sqrt") {
                constraints_.push_back(
                    {ConstraintKind::SqrtNeg, &arg,
                     "sqrt argument " + arg.to_string() + " >= 0"});
            } else if (node.name() == "ln" || node.name() == "log") {
                constraints_.push_back(
                    {ConstraintKind::LogNonPos, &arg,
                     node.name() + " argument " + arg.to_string() + " > 0"});
            }
        }
    };

    // ---------------------------------------------------------------
    // validate_root
    //   Checks whether `value` satisfies all domain constraints when
    //   substituted for `var`.  Returns "" on success, or a human-
    //   readable reason string on failure.
    // ---------------------------------------------------------------
    inline std::string
    validate_root(const std::vector<DomainConstraint>& constraints,
                  const std::string& var, double value, const Context* ctx,
                  const std::string& input) {

        for (const auto& c : constraints) {
            // Evaluate the constraint expression with var = value
            Context temp;
            if (ctx) {
                for (const auto& name : ctx->all_names()) {
                    const Expr* stored = ctx->get_expr(name);
                    if (stored)
                        temp.set(name, stored->clone());
                }
            }
            temp.set(var, value);

            double val = 0.0;
            try {
                Evaluator ev(&temp, input);
                val = ev.evaluate_scalar(*c.expr);
            } catch (...) {
                // If evaluation itself throws, the root is invalid
                return c.description;
            }

            switch (c.kind) {
            case ConstraintKind::DivByZero:
                if (std::abs(val) < 1e-12)
                    return c.description + " (division by zero at " + var +
                           " = " + format_double(value) + ")";
                break;
            case ConstraintKind::SqrtNeg:
                if (val < -1e-12)
                    return c.description + " (negative at " + var + " = " +
                           format_double(value) + ")";
                break;
            case ConstraintKind::LogNonPos:
                if (val <= 1e-12)
                    return c.description + " (non-positive at " + var + " = " +
                           format_double(value) + ")";
                break;
            }
        }
        return ""; // all constraints satisfied
    }

    // Convenience: collect from both sides of an equation
    inline std::vector<DomainConstraint> collect_domain(const Expr& lhs,
                                                        const Expr& rhs) {
        DomainCollector dc;
        dc.collect(lhs);
        auto result = dc.constraints();
        dc.collect(rhs);
        auto rhs_constraints = dc.constraints();
        result.insert(result.end(), rhs_constraints.begin(),
                      rhs_constraints.end());
        return result;
    }

} // namespace math_solver

#endif
