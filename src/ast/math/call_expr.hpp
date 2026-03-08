#pragma once

//! # Module — `src/ast/math/call_expr.hpp`
//!
//! Defines `FuncKind` and `FunctionCall`, the AST node for built-in math
//! function calls such as `sin(x)`, `sqrt(x)`, etc. Part of the math AST
//! layer; produced by the math parser when an identifier followed by `(` is
//! recognised as a known function name.

#include "ast/math/expr.hpp"
#include <optional>
#include <string>
#include <unordered_map>

namespace math_solver {

    /// Enumeration of all supported built-in math functions.
    enum class FuncKind {
        Sin, Cos, Tan,
        Asin, Acos, Atan,
        Sinh, Cosh, Tanh,
        Exp, Sqrt, Ln, Log,
        Abs, Floor, Ceil, Round
    };

    /// Resolve a function name to its `FuncKind`, if known.
    ///
    /// Lookup is performed via a static hash table; the function has O(1)
    /// amortised cost after the first call.
    ///
    /// # Arguments
    ///
    /// * `name` — The raw identifier string as it appears in the source.
    ///   Must exactly match a known function name; no case-folding is applied.
    ///
    /// # Returns
    ///
    /// The corresponding `FuncKind`, or `std::nullopt` for unrecognised names.
    inline std::optional<FuncKind> func_kind_from_name(const std::string& name) {
        static const std::unordered_map<std::string, FuncKind> table = {
            {"sin",   FuncKind::Sin},
            {"cos",   FuncKind::Cos},
            {"tan",   FuncKind::Tan},
            {"asin",  FuncKind::Asin},
            {"acos",  FuncKind::Acos},
            {"atan",  FuncKind::Atan},
            {"sinh",  FuncKind::Sinh},
            {"cosh",  FuncKind::Cosh},
            {"tanh",  FuncKind::Tanh},
            {"exp",   FuncKind::Exp},
            {"sqrt",  FuncKind::Sqrt},
            {"ln",    FuncKind::Ln},
            {"log",   FuncKind::Log},
            {"abs",   FuncKind::Abs},
            {"floor", FuncKind::Floor},
            {"ceil",  FuncKind::Ceil},
            {"round", FuncKind::Round},
        };
        auto it = table.find(name);
        if (it == table.end()) return std::nullopt;
        return it->second;
    }

    /// AST node representing a built-in unary function call.
    ///
    /// Stores the original name string for diagnostics and the resolved
    /// `FuncKind` for fast switch-based dispatch during evaluation. The single
    /// argument is owned as an `ExprPtr` subtree.
    ///
    /// `FuncKind` is resolved at parse time, so evaluation requires only a
    /// switch on the enum — no string hashing at runtime.
    class FunctionCall : public Expr {
        std::string name_;
        FuncKind    kind_;
        ExprPtr     arg_;

    public:
        /// Construct a `FunctionCall` with a resolved function kind and argument.
        ///
        /// # Arguments
        ///
        /// * `name` — The original source name of the function (for diagnostics).
        /// * `kind` — The resolved `FuncKind` (obtained via `func_kind_from_name`).
        /// * `arg`  — The single argument expression; must be non-null.
        /// * `span` — Source region covering the full call expression; defaults
        ///            to an empty span.
        FunctionCall(std::string name, FuncKind kind, ExprPtr arg,
                     const Span& span = Span())
            : Expr(span), name_(std::move(name)), kind_(kind),
              arg_(std::move(arg)) {}

        /// Return the original source name of the function (for diagnostics).
        const std::string& name() const { return name_; }

        /// Return the resolved function kind (for switch dispatch).
        FuncKind           kind() const { return kind_; }

        /// Return the argument expression.
        const Expr&        arg()  const { return *arg_; }

        /// Dispatch to `ExprVisitor::visit(const FunctionCall&)`.
        void accept(ExprVisitor& visitor) const override {
            visitor.visit(*this);
        }

        /// Return a string of the form `"name(arg)"`.
        std::string to_string() const override {
            return name_ + "(" + arg_->to_string() + ")";
        }

        /// Produce a deep copy of this node and its argument subtree.
        ///
        /// # Returns
        ///
        /// A new `FunctionCall` with the same `name_`, `kind_`, `span_`, and an
        /// independent copy of the argument subtree.
        std::unique_ptr<Expr> clone() const override {
            return std::make_unique<FunctionCall>(name_, kind_, arg_->clone(),
                                                  span_);
        }
    };

} // namespace math_solver
