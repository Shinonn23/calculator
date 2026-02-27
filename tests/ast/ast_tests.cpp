// Unit tests for the AST module (math and command nodes).
// Uses a minimal header-only test framework with no external dependencies.

#include <cassert>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

// ── Minimal test framework ──────────────────────────────────────────────────

static int g_pass = 0;
static int g_fail = 0;

#define REQUIRE(cond)                                                          \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::cerr << "  FAIL  " << __FILE__ << ":" << __LINE__            \
                      << "  (" #cond ")\n";                                    \
            ++g_fail;                                                          \
            return;                                                            \
        }                                                                      \
    } while (0)

#define REQUIRE_EQ(a, b)                                                       \
    do {                                                                       \
        auto _a = (a);                                                         \
        auto _b = (b);                                                         \
        if (_a != _b) {                                                        \
            std::cerr << "  FAIL  " << __FILE__ << ":" << __LINE__            \
                      << "  (" #a " == " #b ")  got: " << _a << " vs " << _b  \
                      << "\n";                                                 \
            ++g_fail;                                                          \
            return;                                                            \
        }                                                                      \
    } while (0)

#define TEST(suite, name)                                                      \
    static void test_##suite##_##name();                                       \
    struct _reg_##suite##_##name {                                             \
        _reg_##suite##_##name() {                                              \
            g_tests.push_back({#suite "::" #name, test_##suite##_##name});     \
        }                                                                      \
    } _inst_##suite##_##name;                                                  \
    static void test_##suite##_##name()

struct TestCase {
    std::string   name;
    void        (*fn)();
};
static std::vector<TestCase> g_tests;

static int run_all() {
    for (auto& tc : g_tests) {
        int before = g_fail;
        tc.fn();
        if (g_fail == before) {
            std::cout << "  PASS  " << tc.name << "\n";
            ++g_pass;
        } else {
            std::cout << "  FAIL  " << tc.name << "\n";
        }
    }
    std::cout << "\n" << g_pass << " passed, " << g_fail << " failed\n";
    return g_fail == 0 ? 0 : 1;
}

// ── AST headers ────────────────────────────────────────────────────────────

#include "core/span.hpp"
#include "diagnostics/sink.hpp"
#include "ast/math/binary_expr.hpp"
#include "ast/math/equation_expr.hpp"
#include "ast/math/expr_visitor.hpp"
#include "ast/math/number_expr.hpp"
#include "ast/math/variable_expr.hpp"
#include "ast/command/command.hpp"
#include "ast/command/command_visitor.hpp"
#include "ast/command/config_command.hpp"
#include "ast/command/env_command.hpp"
#include "ast/command/history_command.hpp"
#include "ast/command/history_entry.hpp"
#include "ast/command/load_command.hpp"
#include "ast/command/math_command.hpp"
#include "ast/command/redo_command.hpp"
#include "ast/command/system_command.hpp"
#include "ast/command/var_command.hpp"

using namespace math_solver;

// ── Mock visitors ──────────────────────────────────────────────────────────

// Records which node type was last visited.
struct MockExprVisitor : ExprVisitor {
    enum class LastVisit { None, Number, Variable, BinaryOp };
    LastVisit last = LastVisit::None;

    void visit(const Number&) override   { last = LastVisit::Number; }
    void visit(const BinaryOp&) override { last = LastVisit::BinaryOp; }
    void visit(const Variable&) override { last = LastVisit::Variable; }
};

// Records which command type was last dispatched.
struct MockCommandVisitor : CommandVisitor {
    enum class LastVisit {
        None, System, Var, Math, Env, Config, Load, History, Redo
    };
    LastVisit last = LastVisit::None;

    void visit(const SystemCommand&,  DiagnosticSink&) override { last = LastVisit::System; }
    void visit(const VarCommand&,     DiagnosticSink&) override { last = LastVisit::Var; }
    void visit(const MathCommand&,    DiagnosticSink&) override { last = LastVisit::Math; }
    void visit(const EnvCommand&,     DiagnosticSink&) override { last = LastVisit::Env; }
    void visit(const ConfigCommand&,  DiagnosticSink&) override { last = LastVisit::Config; }
    void visit(const LoadCommand&,    DiagnosticSink&) override { last = LastVisit::Load; }
    void visit(const HistoryCommand&, DiagnosticSink&) override { last = LastVisit::History; }
    void visit(const RedoCommand&,    DiagnosticSink&) override { last = LastVisit::Redo; }
};

// ═══════════════════════════════════════════════════════════════════════════
// core/span.hpp
// ═══════════════════════════════════════════════════════════════════════════

TEST(Span, default_construction) {
    Span s;
    REQUIRE_EQ(s.start, 0u);
    REQUIRE_EQ(s.end,   0u);
}

TEST(Span, parameterized_construction) {
    Span s(3, 7);
    REQUIRE_EQ(s.start, 3u);
    REQUIRE_EQ(s.end,   7u);
}

TEST(Span, length) {
    Span s(2, 9);
    REQUIRE_EQ(s.length(), 7u);
}

TEST(Span, empty_true) {
    Span s(4, 4);
    REQUIRE(s.empty());
}

TEST(Span, empty_false) {
    Span s(1, 5);
    REQUIRE(!s.empty());
}

TEST(Span, merge_disjoint) {
    Span a(1, 4), b(6, 10);
    Span m = a.merge(b);
    REQUIRE_EQ(m.start, 1u);
    REQUIRE_EQ(m.end,  10u);
}

TEST(Span, merge_overlapping) {
    Span a(2, 8), b(5, 12);
    Span m = a.merge(b);
    REQUIRE_EQ(m.start, 2u);
    REQUIRE_EQ(m.end,  12u);
}

TEST(Span, merge_self) {
    Span a(3, 7);
    Span m = a.merge(a);
    REQUIRE_EQ(m.start, 3u);
    REQUIRE_EQ(m.end,   7u);
}

TEST(Span, merge_adjacent) {
    Span a(0, 3), b(3, 6);
    Span m = a.merge(b);
    REQUIRE_EQ(m.start, 0u);
    REQUIRE_EQ(m.end,   6u);
}

// ═══════════════════════════════════════════════════════════════════════════
// ast/math/number_expr.hpp
// ═══════════════════════════════════════════════════════════════════════════

TEST(Number, value_stored) {
    Number n(3.14);
    REQUIRE(std::abs(n.value() - 3.14) < 1e-9);
}

TEST(Number, span_defaults_empty) {
    Number n(1.0);
    REQUIRE(n.span().empty());
}

TEST(Number, span_explicit) {
    Number n(1.0, Span(2, 5));
    REQUIRE_EQ(n.span().start, 2u);
    REQUIRE_EQ(n.span().end,   5u);
}

TEST(Number, to_string_integer_trims_decimal) {
    Number n(5.0);
    REQUIRE_EQ(n.to_string(), std::string("5"));
}

TEST(Number, to_string_trims_trailing_zeros) {
    Number n(1.5);
    std::string s = n.to_string();
    // Must not end with '0' after the decimal point.
    REQUIRE(s.back() != '0');
    REQUIRE(s.find("1.5") != std::string::npos);
}

TEST(Number, to_string_zero) {
    Number n(0.0);
    REQUIRE_EQ(n.to_string(), std::string("0"));
}

TEST(Number, to_string_negative) {
    Number n(-2.0);
    REQUIRE_EQ(n.to_string(), std::string("-2"));
}

TEST(Number, clone_preserves_value) {
    Number n(42.0, Span(1, 3));
    auto   c = n.clone();
    REQUIRE(std::abs(static_cast<Number&>(*c).value() - 42.0) < 1e-9);
}

TEST(Number, clone_preserves_span) {
    Number n(7.0, Span(4, 6));
    auto   c = n.clone();
    REQUIRE_EQ(c->span().start, 4u);
    REQUIRE_EQ(c->span().end,   6u);
}

TEST(Number, clone_is_independent) {
    Number n(1.0);
    auto   c = n.clone();
    REQUIRE(c.get() != &n);
}

TEST(Number, accept_dispatches_to_visitor) {
    Number          n(1.0);
    MockExprVisitor v;
    n.accept(v);
    REQUIRE(v.last == MockExprVisitor::LastVisit::Number);
}

// ═══════════════════════════════════════════════════════════════════════════
// ast/math/variable_expr.hpp
// ═══════════════════════════════════════════════════════════════════════════

TEST(Variable, name_stored) {
    Variable v("x");
    REQUIRE_EQ(v.name(), std::string("x"));
}

TEST(Variable, span_defaults_empty) {
    Variable v("y");
    REQUIRE(v.span().empty());
}

TEST(Variable, span_explicit) {
    Variable v("z", Span(1, 2));
    REQUIRE_EQ(v.span().start, 1u);
    REQUIRE_EQ(v.span().end,   2u);
}

TEST(Variable, to_string_equals_name) {
    Variable v("alpha");
    REQUIRE_EQ(v.to_string(), std::string("alpha"));
}

TEST(Variable, clone_preserves_name_and_span) {
    Variable v("beta", Span(3, 7));
    auto     c = v.clone();
    auto&    cv = static_cast<Variable&>(*c);
    REQUIRE_EQ(cv.name(),       std::string("beta"));
    REQUIRE_EQ(cv.span().start, 3u);
    REQUIRE_EQ(cv.span().end,   7u);
}

TEST(Variable, clone_is_independent) {
    Variable v("x");
    auto     c = v.clone();
    REQUIRE(c.get() != &v);
}

TEST(Variable, accept_dispatches_to_visitor) {
    Variable        v("x");
    MockExprVisitor vis;
    v.accept(vis);
    REQUIRE(vis.last == MockExprVisitor::LastVisit::Variable);
}

// ═══════════════════════════════════════════════════════════════════════════
// ast/math/binary_expr.hpp
// ═══════════════════════════════════════════════════════════════════════════

static ExprPtr make_num(double v, size_t s = 0, size_t e = 0) {
    return std::make_unique<Number>(v, Span(s, e));
}
static ExprPtr make_var(const std::string& n, size_t s = 0, size_t e = 0) {
    return std::make_unique<Variable>(n, Span(s, e));
}

TEST(BinaryOp, op_stored) {
    BinaryOp b(make_num(1), make_num(2), BinaryOpType::Add);
    REQUIRE(b.op() == BinaryOpType::Add);
}

TEST(BinaryOp, left_right_stored) {
    BinaryOp b(make_num(3), make_var("x"), BinaryOpType::Mul);
    REQUIRE_EQ(b.left().to_string(),  std::string("3"));
    REQUIRE_EQ(b.right().to_string(), std::string("x"));
}

TEST(BinaryOp, span_auto_merges_children) {
    BinaryOp b(make_num(1, 0, 1), make_num(2, 4, 5), BinaryOpType::Add);
    REQUIRE_EQ(b.span().start, 0u);
    REQUIRE_EQ(b.span().end,   5u);
}

TEST(BinaryOp, span_explicit_overrides) {
    BinaryOp b(make_num(1, 0, 1), make_num(2, 4, 5), BinaryOpType::Sub,
               Span(0, 10));
    REQUIRE_EQ(b.span().start, 0u);
    REQUIRE_EQ(b.span().end,  10u);
}

TEST(BinaryOp, to_string_add) {
    BinaryOp b(make_num(1), make_num(2), BinaryOpType::Add);
    REQUIRE_EQ(b.to_string(), std::string("(1 + 2)"));
}

TEST(BinaryOp, to_string_sub) {
    BinaryOp b(make_num(5), make_num(3), BinaryOpType::Sub);
    REQUIRE_EQ(b.to_string(), std::string("(5 - 3)"));
}

TEST(BinaryOp, to_string_mul) {
    BinaryOp b(make_var("a"), make_var("b"), BinaryOpType::Mul);
    REQUIRE_EQ(b.to_string(), std::string("(a * b)"));
}

TEST(BinaryOp, to_string_div) {
    BinaryOp b(make_num(6), make_num(2), BinaryOpType::Div);
    REQUIRE_EQ(b.to_string(), std::string("(6 / 2)"));
}

TEST(BinaryOp, to_string_pow) {
    BinaryOp b(make_var("x"), make_num(2), BinaryOpType::Pow);
    REQUIRE_EQ(b.to_string(), std::string("(x ^ 2)"));
}

TEST(BinaryOp, to_string_nested) {
    auto inner = std::make_unique<BinaryOp>(make_num(1), make_num(2), BinaryOpType::Add);
    BinaryOp outer(std::move(inner), make_num(3), BinaryOpType::Mul);
    REQUIRE_EQ(outer.to_string(), std::string("((1 + 2) * 3)"));
}

TEST(BinaryOp, clone_preserves_op_and_operands) {
    BinaryOp  b(make_num(4), make_var("y"), BinaryOpType::Div, Span(0, 5));
    auto      c = b.clone();
    auto&     cb = static_cast<BinaryOp&>(*c);
    REQUIRE(cb.op() == BinaryOpType::Div);
    REQUIRE_EQ(cb.left().to_string(),  std::string("4"));
    REQUIRE_EQ(cb.right().to_string(), std::string("y"));
}

TEST(BinaryOp, clone_preserves_span) {
    BinaryOp b(make_num(1, 0, 1), make_num(2, 2, 3), BinaryOpType::Add);
    auto     c = b.clone();
    REQUIRE_EQ(c->span().start, b.span().start);
    REQUIRE_EQ(c->span().end,   b.span().end);
}

TEST(BinaryOp, clone_is_independent) {
    BinaryOp b(make_num(1), make_num(2), BinaryOpType::Add);
    auto     c = b.clone();
    REQUIRE(c.get() != static_cast<Expr*>(&b));
}

TEST(BinaryOp, accept_dispatches_to_visitor) {
    BinaryOp        b(make_num(1), make_num(2), BinaryOpType::Add);
    MockExprVisitor v;
    b.accept(v);
    REQUIRE(v.last == MockExprVisitor::LastVisit::BinaryOp);
}

TEST(BinaryOp, all_optypes_covered) {
    // Smoke: to_string must not return "?" for any known op type.
    for (auto op : {BinaryOpType::Add, BinaryOpType::Sub, BinaryOpType::Mul,
                    BinaryOpType::Div, BinaryOpType::Pow}) {
        BinaryOp b(make_num(1), make_num(2), op);
        std::string s = b.to_string();
        REQUIRE(s.find('?') == std::string::npos);
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// ast/math/equation_expr.hpp
// ═══════════════════════════════════════════════════════════════════════════

TEST(Equation, lhs_rhs_stored) {
    Equation eq(make_num(1), make_var("x"));
    REQUIRE_EQ(eq.lhs().to_string(), std::string("1"));
    REQUIRE_EQ(eq.rhs().to_string(), std::string("x"));
}

TEST(Equation, span_auto_merges_children) {
    Equation eq(make_num(0, 0, 1), make_var("x", 4, 5));
    REQUIRE_EQ(eq.span().start, 0u);
    REQUIRE_EQ(eq.span().end,   5u);
}

TEST(Equation, span_explicit) {
    Equation eq(make_num(0, 0, 1), make_var("x", 4, 5), Span(0, 10));
    REQUIRE_EQ(eq.span().start, 0u);
    REQUIRE_EQ(eq.span().end,  10u);
}

TEST(Equation, to_string) {
    Equation eq(make_num(3), make_var("x"));
    REQUIRE_EQ(eq.to_string(), std::string("3 = x"));
}

TEST(Equation, clone_deep_copy) {
    Equation eq(make_num(2), make_var("y"), Span(0, 5));
    auto     c = eq.clone();
    REQUIRE(c.get() != &eq);
    REQUIRE_EQ(c->to_string(),   std::string("2 = y"));
    REQUIRE_EQ(c->span().start,  0u);
    REQUIRE_EQ(c->span().end,    5u);
}

TEST(Equation, take_lhs_transfers_ownership) {
    Equation eq(make_num(7), make_var("z"));
    auto     lhs = eq.take_lhs();
    REQUIRE(lhs != nullptr);
    REQUIRE_EQ(lhs->to_string(), std::string("7"));
}

TEST(Equation, take_rhs_transfers_ownership) {
    Equation eq(make_num(1), make_var("w"));
    auto     rhs = eq.take_rhs();
    REQUIRE(rhs != nullptr);
    REQUIRE_EQ(rhs->to_string(), std::string("w"));
}

// ═══════════════════════════════════════════════════════════════════════════
// ExprVisitor dispatch correctness
// ═══════════════════════════════════════════════════════════════════════════

TEST(ExprVisitor, number_dispatches_number) {
    MockExprVisitor v;
    Number n(1.0);
    n.accept(v);
    REQUIRE(v.last == MockExprVisitor::LastVisit::Number);
}

TEST(ExprVisitor, variable_dispatches_variable) {
    MockExprVisitor v;
    Variable var("x");
    var.accept(v);
    REQUIRE(v.last == MockExprVisitor::LastVisit::Variable);
}

TEST(ExprVisitor, binary_op_dispatches_binary_op) {
    MockExprVisitor v;
    BinaryOp b(make_num(1), make_num(2), BinaryOpType::Add);
    b.accept(v);
    REQUIRE(v.last == MockExprVisitor::LastVisit::BinaryOp);
}

TEST(ExprVisitor, dispatch_updates_last_visited) {
    MockExprVisitor v;
    Number   n(1.0);
    Variable var("x");
    n.accept(v);
    REQUIRE(v.last == MockExprVisitor::LastVisit::Number);
    var.accept(v);
    REQUIRE(v.last == MockExprVisitor::LastVisit::Variable);
}

// ═══════════════════════════════════════════════════════════════════════════
// ast/command/history_entry.hpp  — HistoryStatus + HistoryEntry
// ═══════════════════════════════════════════════════════════════════════════

TEST(HistoryStatus, to_string_success) {
    REQUIRE_EQ(to_string(HistoryStatus::Success), std::string("success"));
}

TEST(HistoryStatus, to_string_error) {
    REQUIRE_EQ(to_string(HistoryStatus::Error), std::string("error"));
}

TEST(HistoryStatus, to_string_warning) {
    REQUIRE_EQ(to_string(HistoryStatus::Warning), std::string("warning"));
}

TEST(HistoryStatus, to_string_info) {
    REQUIRE_EQ(to_string(HistoryStatus::Info), std::string("info"));
}

TEST(HistoryStatus, to_string_unknown) {
    REQUIRE_EQ(to_string(HistoryStatus::Unknown), std::string("unknown"));
}

TEST(HistoryStatus, parse_success) {
    REQUIRE(parse_history_status("success") == HistoryStatus::Success);
}

TEST(HistoryStatus, parse_error) {
    REQUIRE(parse_history_status("error") == HistoryStatus::Error);
}

TEST(HistoryStatus, parse_warning) {
    REQUIRE(parse_history_status("warning") == HistoryStatus::Warning);
}

TEST(HistoryStatus, parse_info) {
    REQUIRE(parse_history_status("info") == HistoryStatus::Info);
}

TEST(HistoryStatus, parse_unknown_string) {
    REQUIRE(parse_history_status("garbage") == HistoryStatus::Unknown);
    REQUIRE(parse_history_status("") == HistoryStatus::Unknown);
}

TEST(HistoryStatus, roundtrip) {
    for (auto s : {HistoryStatus::Success, HistoryStatus::Error,
                   HistoryStatus::Warning, HistoryStatus::Info}) {
        REQUIRE(parse_history_status(to_string(s)) == s);
    }
}

TEST(HistoryEntry, default_status_is_success) {
    HistoryEntry e{"cmd", HistoryStatus::Success, ""};
    REQUIRE(e.is_success());
    REQUIRE(!e.is_error());
    REQUIRE(!e.is_warning());
    REQUIRE(!e.is_info());
    REQUIRE(!e.failed());
}

TEST(HistoryEntry, error_predicates) {
    HistoryEntry e{"cmd", HistoryStatus::Error, ""};
    REQUIRE(!e.is_success());
    REQUIRE(e.is_error());
    REQUIRE(e.failed());
}

TEST(HistoryEntry, warning_predicates) {
    HistoryEntry e{"cmd", HistoryStatus::Warning, ""};
    REQUIRE(e.is_warning());
    REQUIRE(!e.is_success());
    REQUIRE(!e.failed());
}

TEST(HistoryEntry, info_predicates) {
    HistoryEntry e{"cmd", HistoryStatus::Info, ""};
    REQUIRE(e.is_info());
    REQUIRE(!e.is_success());
}

// ═══════════════════════════════════════════════════════════════════════════
// ast/command/command.hpp  — base Command
// ═══════════════════════════════════════════════════════════════════════════

TEST(Command, raw_command_stored) {
    SystemCommand c(SystemCommand::Type::Exit, "exit");
    REQUIRE_EQ(c.raw_command(), std::string("exit"));
}

TEST(Command, source_file_defaults_to_repl) {
    SystemCommand c(SystemCommand::Type::Exit, "exit");
    REQUIRE_EQ(c.source_file(), std::string("<repl>"));
}

TEST(Command, source_line_defaults_to_1) {
    SystemCommand c(SystemCommand::Type::Exit, "exit");
    REQUIRE_EQ(c.source_line(), 1u);
}

TEST(Command, set_source_updates_file_and_line) {
    SystemCommand c(SystemCommand::Type::Exit, "exit");
    c.set_source("script.msl", 42);
    REQUIRE_EQ(c.source_file(), std::string("script.msl"));
    REQUIRE_EQ(c.source_line(), 42u);
}

// ═══════════════════════════════════════════════════════════════════════════
// ast/command/system_command.hpp
// ═══════════════════════════════════════════════════════════════════════════

TEST(SystemCommand, type_exit) {
    SystemCommand c(SystemCommand::Type::Exit, "exit");
    REQUIRE(c.type() == SystemCommand::Type::Exit);
}

TEST(SystemCommand, type_help) {
    SystemCommand c(SystemCommand::Type::Help, "help");
    REQUIRE(c.type() == SystemCommand::Type::Help);
}

TEST(SystemCommand, type_clear) {
    SystemCommand c(SystemCommand::Type::Clear, "clear");
    REQUIRE(c.type() == SystemCommand::Type::Clear);
}

TEST(SystemCommand, type_ls) {
    SystemCommand c(SystemCommand::Type::Ls, "ls");
    REQUIRE(c.type() == SystemCommand::Type::Ls);
}

TEST(SystemCommand, type_unknown) {
    SystemCommand c(SystemCommand::Type::Unknown, "???");
    REQUIRE(c.type() == SystemCommand::Type::Unknown);
}

TEST(SystemCommand, accept_dispatches_system) {
    SystemCommand        c(SystemCommand::Type::Exit, "exit");
    MockCommandVisitor   v;
    DiagnosticSink       sink;
    c.accept(v, sink);
    REQUIRE(v.last == MockCommandVisitor::LastVisit::System);
}

// ═══════════════════════════════════════════════════════════════════════════
// ast/command/math_command.hpp
// ═══════════════════════════════════════════════════════════════════════════

TEST(MathCommand, type_stored) {
    MathCommand c(MathCommand::Type::Solve, "x+1=0", "solve x+1=0");
    REQUIRE(c.type() == MathCommand::Type::Solve);
}

TEST(MathCommand, payload_stored) {
    MathCommand c(MathCommand::Type::Evaluate, "2+2", "eval 2+2");
    REQUIRE_EQ(c.payload(), std::string("2+2"));
}

TEST(MathCommand, flags_default_false) {
    MathCommand c(MathCommand::Type::Evaluate, "1", "eval 1");
    REQUIRE(!c.isolated());
    REQUIRE(!c.as_fraction());
    REQUIRE(c.specific_vars().empty());
}

TEST(MathCommand, set_flags_isolated) {
    MathCommand c(MathCommand::Type::Solve, "x=1", "solve x=1");
    c.set_flags(true, false);
    REQUIRE(c.isolated());
    REQUIRE(!c.as_fraction());
}

TEST(MathCommand, set_flags_fraction) {
    MathCommand c(MathCommand::Type::Simplify, "a/b", "simplify a/b");
    c.set_flags(false, true);
    REQUIRE(!c.isolated());
    REQUIRE(c.as_fraction());
}

TEST(MathCommand, set_flags_specific_vars) {
    MathCommand c(MathCommand::Type::Solve, "x+y=1", "solve x+y=1");
    c.set_flags(false, false, {"x", "y"});
    auto& vars = c.specific_vars();
    REQUIRE_EQ(vars.size(), 2u);
    REQUIRE_EQ(vars[0], std::string("x"));
    REQUIRE_EQ(vars[1], std::string("y"));
}

TEST(MathCommand, all_types_constructible) {
    for (auto t : {MathCommand::Type::Evaluate, MathCommand::Type::Solve,
                   MathCommand::Type::Simplify, MathCommand::Type::Expand,
                   MathCommand::Type::Factor,   MathCommand::Type::Unknown}) {
        MathCommand c(t, "", "");
        REQUIRE(c.type() == t);
    }
}

TEST(MathCommand, accept_dispatches_math) {
    MathCommand          c(MathCommand::Type::Evaluate, "1", "eval 1");
    MockCommandVisitor   v;
    DiagnosticSink       sink;
    c.accept(v, sink);
    REQUIRE(v.last == MockCommandVisitor::LastVisit::Math);
}

// ═══════════════════════════════════════════════════════════════════════════
// ast/command/var_command.hpp
// ═══════════════════════════════════════════════════════════════════════════

TEST(VarCommand, action_set) {
    VarCommand c(VarCommand::Action::Set, "x", "var x = 1");
    REQUIRE(c.action() == VarCommand::Action::Set);
}

TEST(VarCommand, action_unset) {
    VarCommand c(VarCommand::Action::Unset, "x", "var x unset");
    REQUIRE(c.action() == VarCommand::Action::Unset);
}

TEST(VarCommand, var_name_stored) {
    VarCommand c(VarCommand::Action::Set, "myVar", "var myVar = 3");
    REQUIRE_EQ(c.var_name(), std::string("myVar"));
}

TEST(VarCommand, no_payload_by_default) {
    VarCommand c(VarCommand::Action::Set, "x", "var x");
    REQUIRE(!c.has_payload());
    REQUIRE(!c.has_math_action());
}

TEST(VarCommand, set_payload_stores_action_and_payload) {
    VarCommand c(VarCommand::Action::Set, "x", "var x solve x+1=0");
    c.set_payload("solve", "x+1=0");
    REQUIRE(c.has_payload());
    REQUIRE(c.has_math_action());
    REQUIRE_EQ(c.payload(),      std::string("x+1=0"));
    REQUIRE_EQ(c.math_action(),  std::string("solve"));
}

TEST(VarCommand, accept_dispatches_var) {
    VarCommand           c(VarCommand::Action::Set, "x", "var x = 1");
    MockCommandVisitor   v;
    DiagnosticSink       sink;
    c.accept(v, sink);
    REQUIRE(v.last == MockCommandVisitor::LastVisit::Var);
}

// ═══════════════════════════════════════════════════════════════════════════
// ast/command/config_command.hpp
// ═══════════════════════════════════════════════════════════════════════════

TEST(ConfigCommand, action_list) {
    ConfigCommand c(ConfigCommand::Action::List, "config list");
    REQUIRE(c.action() == ConfigCommand::Action::List);
}

TEST(ConfigCommand, action_get) {
    ConfigCommand c(ConfigCommand::Action::Get, "config get k");
    REQUIRE(c.action() == ConfigCommand::Action::Get);
}

TEST(ConfigCommand, action_set) {
    ConfigCommand c(ConfigCommand::Action::Set, "config set k v");
    REQUIRE(c.action() == ConfigCommand::Action::Set);
}

TEST(ConfigCommand, action_path) {
    ConfigCommand c(ConfigCommand::Action::Path, "config path");
    REQUIRE(c.action() == ConfigCommand::Action::Path);
}

TEST(ConfigCommand, action_reset) {
    ConfigCommand c(ConfigCommand::Action::Reset, "config reset");
    REQUIRE(c.action() == ConfigCommand::Action::Reset);
}

TEST(ConfigCommand, kv_default_empty) {
    ConfigCommand c(ConfigCommand::Action::List, "config list");
    REQUIRE(c.key().empty());
    REQUIRE(c.value().empty());
}

TEST(ConfigCommand, set_kv_key_only) {
    ConfigCommand c(ConfigCommand::Action::Get, "config get theme");
    c.set_kv("theme");
    REQUIRE_EQ(c.key(),   std::string("theme"));
    REQUIRE(c.value().empty());
}

TEST(ConfigCommand, set_kv_key_and_value) {
    ConfigCommand c(ConfigCommand::Action::Set, "config set theme dark");
    c.set_kv("theme", "dark");
    REQUIRE_EQ(c.key(),   std::string("theme"));
    REQUIRE_EQ(c.value(), std::string("dark"));
}

TEST(ConfigCommand, accept_dispatches_config) {
    ConfigCommand        c(ConfigCommand::Action::List, "config list");
    MockCommandVisitor   v;
    DiagnosticSink       sink;
    c.accept(v, sink);
    REQUIRE(v.last == MockCommandVisitor::LastVisit::Config);
}

// ═══════════════════════════════════════════════════════════════════════════
// ast/command/env_command.hpp
// ═══════════════════════════════════════════════════════════════════════════

TEST(EnvCommand, action_stored) {
    EnvCommand c(EnvCommand::Action::List, "", "env list");
    REQUIRE(c.action() == EnvCommand::Action::List);
}

TEST(EnvCommand, target_env_stored) {
    EnvCommand c(EnvCommand::Action::Load, "work", "env load work");
    REQUIRE_EQ(c.target_env(), std::string("work"));
}

TEST(EnvCommand, source_env_stored) {
    EnvCommand c(EnvCommand::Action::Move, "dst", "env move src dst");
    c.set_source_env("src");
    REQUIRE_EQ(c.source_env(), std::string("src"));
}

TEST(EnvCommand, vars_to_save_empty_by_default) {
    EnvCommand c(EnvCommand::Action::Save, "env1", "env save env1");
    REQUIRE(c.vars_to_save().empty());
}

TEST(EnvCommand, set_vars_to_save) {
    EnvCommand c(EnvCommand::Action::Save, "env1", "env save env1");
    c.set_vars_to_save({"x", "y"});
    REQUIRE_EQ(c.vars_to_save().size(), 2u);
    REQUIRE_EQ(c.vars_to_save()[0], std::string("x"));
}

TEST(EnvCommand, flags_default) {
    EnvCommand c(EnvCommand::Action::Copy, "dst", "env copy src dst");
    REQUIRE(!c.flags().vars_mode);
    REQUIRE(c.flags().to_env.empty());
}

TEST(EnvCommand, set_flags) {
    EnvCommand c(EnvCommand::Action::Move, "dst", "env move src dst");
    EnvCommand::Flags flags{true, "other"};
    c.set_flags(flags);
    REQUIRE(c.flags().vars_mode);
    REQUIRE_EQ(c.flags().to_env, std::string("other"));
}

TEST(EnvCommand, all_actions_constructible) {
    for (auto a : {EnvCommand::Action::Show,    EnvCommand::Action::List,
                   EnvCommand::Action::Load,    EnvCommand::Action::Save,
                   EnvCommand::Action::New,     EnvCommand::Action::Delete,
                   EnvCommand::Action::Move,    EnvCommand::Action::Copy,
                   EnvCommand::Action::Unknown}) {
        EnvCommand c(a, "", "");
        REQUIRE(c.action() == a);
    }
}

TEST(EnvCommand, accept_dispatches_env) {
    EnvCommand           c(EnvCommand::Action::List, "", "env list");
    MockCommandVisitor   v;
    DiagnosticSink       sink;
    c.accept(v, sink);
    REQUIRE(v.last == MockCommandVisitor::LastVisit::Env);
}

// ═══════════════════════════════════════════════════════════════════════════
// ast/command/load_command.hpp
// ═══════════════════════════════════════════════════════════════════════════

TEST(LoadCommand, filepath_stored) {
    LoadCommand c("script.msl", {}, "load script.msl");
    REQUIRE_EQ(c.filepath(), std::string("script.msl"));
}

TEST(LoadCommand, flags_defaults) {
    LoadCommand c("f.msl", {}, "load f.msl");
    REQUIRE(!c.flags().dry_run);
    REQUIRE(!c.flags().silent);
    REQUIRE(!c.flags().strict);
    REQUIRE(!c.flags().no_rollback);
    REQUIRE(c.flags().env.empty());
}

TEST(LoadCommand, flags_set) {
    LoadCommand::Flags f{true, true, true, true, "myenv"};
    LoadCommand        c("f.msl", f, "load f.msl");
    REQUIRE(c.flags().dry_run);
    REQUIRE(c.flags().silent);
    REQUIRE(c.flags().strict);
    REQUIRE(c.flags().no_rollback);
    REQUIRE_EQ(c.flags().env, std::string("myenv"));
}

TEST(LoadCommand, accept_dispatches_load) {
    LoadCommand          c("f.msl", {}, "load f.msl");
    MockCommandVisitor   v;
    DiagnosticSink       sink;
    c.accept(v, sink);
    REQUIRE(v.last == MockCommandVisitor::LastVisit::Load);
}

// ═══════════════════════════════════════════════════════════════════════════
// ast/command/history_command.hpp
// ═══════════════════════════════════════════════════════════════════════════

TEST(HistoryCommand, action_stored) {
    HistoryCommand c(HistoryCommand::Action::Show, "history");
    REQUIRE(c.action() == HistoryCommand::Action::Show);
}

TEST(HistoryCommand, limit_default_20) {
    HistoryCommand c(HistoryCommand::Action::Show, "history");
    REQUIRE_EQ(c.limit(), 20);
}

TEST(HistoryCommand, set_limit) {
    HistoryCommand c(HistoryCommand::Action::Show, "history 5");
    c.set_limit(5);
    REQUIRE_EQ(c.limit(), 5);
}

TEST(HistoryCommand, set_pattern) {
    HistoryCommand c(HistoryCommand::Action::Search, "history search foo");
    c.set_pattern("foo");
    REQUIRE_EQ(c.pattern(), std::string("foo"));
}

TEST(HistoryCommand, set_filepath) {
    HistoryCommand c(HistoryCommand::Action::Save, "history save out.txt");
    c.set_filepath("out.txt");
    REQUIRE_EQ(c.filepath(), std::string("out.txt"));
}

TEST(HistoryCommand, set_range) {
    HistoryCommand c(HistoryCommand::Action::ShowRange, "history 1-3");
    c.set_range({1, 2, 3});
    REQUIRE_EQ(c.range().size(), 3u);
    REQUIRE_EQ(c.range()[0], 1);
    REQUIRE_EQ(c.range()[2], 3);
}

TEST(HistoryCommand, is_readonly_show) {
    HistoryCommand c(HistoryCommand::Action::Show, "history");
    REQUIRE(c.is_readonly());
}

TEST(HistoryCommand, is_readonly_show_range) {
    HistoryCommand c(HistoryCommand::Action::ShowRange, "history 1-3");
    REQUIRE(c.is_readonly());
}

TEST(HistoryCommand, is_readonly_search) {
    HistoryCommand c(HistoryCommand::Action::Search, "history search x");
    REQUIRE(c.is_readonly());
}

TEST(HistoryCommand, not_readonly_save) {
    HistoryCommand c(HistoryCommand::Action::Save, "history save f");
    REQUIRE(!c.is_readonly());
}

TEST(HistoryCommand, not_readonly_clear) {
    HistoryCommand c(HistoryCommand::Action::Clear, "history clear");
    REQUIRE(!c.is_readonly());
}

TEST(HistoryCommand, flags_empty_by_default) {
    HistoryCommand c(HistoryCommand::Action::Show, "history");
    REQUIRE(!c.has_any_flag());
}

TEST(HistoryCommand, set_flags_errors) {
    HistoryCommand c(HistoryCommand::Action::Show, "history");
    c.set_flags({HistoryCommand::Flag::Errors});
    REQUIRE(c.has_any_flag());
    REQUIRE(c.has_flag(HistoryCommand::Flag::Errors));
    REQUIRE(!c.has_flag(HistoryCommand::Flag::Success));
}

TEST(HistoryCommand, set_flags_multiple) {
    HistoryCommand c(HistoryCommand::Action::Show, "history");
    c.set_flags({HistoryCommand::Flag::Errors, HistoryCommand::Flag::Warning});
    REQUIRE(c.has_flag(HistoryCommand::Flag::Errors));
    REQUIRE(c.has_flag(HistoryCommand::Flag::Warning));
    REQUIRE(!c.has_flag(HistoryCommand::Flag::Success));
}

TEST(HistoryCommand, set_flags_none_still_stored) {
    HistoryCommand c(HistoryCommand::Action::Show, "history");
    c.set_flags({HistoryCommand::Flag::None});
    REQUIRE(c.has_any_flag());
    REQUIRE(c.has_flag(HistoryCommand::Flag::None));
}

TEST(HistoryCommand, accept_dispatches_history) {
    HistoryCommand       c(HistoryCommand::Action::Show, "history");
    MockCommandVisitor   v;
    DiagnosticSink       sink;
    c.accept(v, sink);
    REQUIRE(v.last == MockCommandVisitor::LastVisit::History);
}

// ═══════════════════════════════════════════════════════════════════════════
// ast/command/redo_command.hpp
// ═══════════════════════════════════════════════════════════════════════════

TEST(RedoCommand, range_empty_by_default) {
    RedoCommand c("redo");
    REQUIRE(c.range().empty());
}

TEST(RedoCommand, set_range) {
    RedoCommand c("redo 2-4");
    c.set_range({2, 3, 4});
    REQUIRE_EQ(c.range().size(), 3u);
    REQUIRE_EQ(c.range()[0], 2);
    REQUIRE_EQ(c.range()[2], 4);
}

TEST(RedoCommand, raw_command_stored) {
    RedoCommand c("redo");
    REQUIRE_EQ(c.raw_command(), std::string("redo"));
}

TEST(RedoCommand, accept_dispatches_redo) {
    RedoCommand          c("redo");
    MockCommandVisitor   v;
    DiagnosticSink       sink;
    c.accept(v, sink);
    REQUIRE(v.last == MockCommandVisitor::LastVisit::Redo);
}

// ═══════════════════════════════════════════════════════════════════════════
// main
// ═══════════════════════════════════════════════════════════════════════════

int main() {
    std::cout << "=== AST Unit Tests ===\n\n";
    return run_all();
}
