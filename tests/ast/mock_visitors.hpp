#pragma once

#include "ast/command/command_visitor.hpp"
#include "ast/command/config_command.hpp"
#include "ast/command/env_command.hpp"
#include "ast/command/history_command.hpp"
#include "ast/command/load_command.hpp"
#include "ast/command/math_command.hpp"
#include "ast/command/redo_command.hpp"
#include "ast/command/system_command.hpp"
#include "ast/command/var_command.hpp"
#include "ast/math/array_expr.hpp"
#include "ast/math/binary_expr.hpp"
#include "ast/math/expr_visitor.hpp"
#include "ast/math/number_expr.hpp"
#include "ast/math/unary_expr.hpp"
#include "ast/math/variable_expr.hpp"
#include "diagnostics/sink.hpp"

namespace test_helpers {

using namespace math_solver;

struct MockExprVisitor : ExprVisitor {
    enum class Visited { None, Number, Variable, BinaryOp, UnaryOp, Array };
    Visited last = Visited::None;

    void visit(const Number&) override   { last = Visited::Number; }
    void visit(const BinaryOp&) override { last = Visited::BinaryOp; }
    void visit(const UnaryOp&) override  { last = Visited::UnaryOp; }
    void visit(const Variable&) override { last = Visited::Variable; }
    void visit(const ArrayExpr&) override { last = Visited::Array; }
};

struct MockCommandVisitor : CommandVisitor {
    enum class Visited { None, System, Var, Math, Env, Config, Load, History, Redo };
    Visited last = Visited::None;

    void visit(const SystemCommand&,  DiagnosticSink&) override { last = Visited::System; }
    void visit(const VarCommand&,     DiagnosticSink&) override { last = Visited::Var; }
    void visit(const MathCommand&,    DiagnosticSink&) override { last = Visited::Math; }
    void visit(const EnvCommand&,     DiagnosticSink&) override { last = Visited::Env; }
    void visit(const ConfigCommand&,  DiagnosticSink&) override { last = Visited::Config; }
    void visit(const LoadCommand&,    DiagnosticSink&) override { last = Visited::Load; }
    void visit(const HistoryCommand&, DiagnosticSink&) override { last = Visited::History; }
    void visit(const RedoCommand&,    DiagnosticSink&) override { last = Visited::Redo; }
};

} // namespace test_helpers
