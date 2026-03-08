#include <gtest/gtest.h>

#include "algebra/linear/linear_collector.hpp"
#include "ast/math/array_expr.hpp"
#include "ast/math/binary_expr.hpp"
#include "ast/math/call_expr.hpp"
#include "ast/math/number_expr.hpp"
#include "ast/math/unary_expr.hpp"
#include "ast/math/variable_expr.hpp"

using namespace math_solver;

// ============================================================
// LinearForm
// ============================================================

TEST(LinearForm, DefaultIsZero) {
    LinearForm f;
    EXPECT_TRUE(f.is_constant());
    EXPECT_DOUBLE_EQ(f.constant, 0.0);
    EXPECT_TRUE(f.coeffs.empty());
}

TEST(LinearForm, ConstantConstruction) {
    LinearForm f(3.5);
    EXPECT_TRUE(f.is_constant());
    EXPECT_DOUBLE_EQ(f.constant, 3.5);
}

TEST(LinearForm, VariableConstruction) {
    LinearForm f("x");
    EXPECT_FALSE(f.is_constant());
    EXPECT_DOUBLE_EQ(f.get_coeff("x"), 1.0);
    EXPECT_DOUBLE_EQ(f.constant, 0.0);
}

TEST(LinearForm, VariableWithCoeff) {
    LinearForm f("x", 3.0);
    EXPECT_DOUBLE_EQ(f.get_coeff("x"), 3.0);
}

TEST(LinearForm, GetCoeffMissing) {
    LinearForm f("x");
    EXPECT_DOUBLE_EQ(f.get_coeff("y"), 0.0);
}

TEST(LinearForm, Variables) {
    LinearForm f("x");
    f.coeffs["y"] = 2.0;
    auto vars = f.variables();
    EXPECT_EQ(vars.count("x"), 1u);
    EXPECT_EQ(vars.count("y"), 1u);
}

TEST(LinearForm, VariablesExcludesNearZero) {
    LinearForm f("x");
    f.coeffs["y"] = 1e-15; // below kEpsilon
    auto vars = f.variables();
    EXPECT_EQ(vars.count("y"), 0u);
}

TEST(LinearForm, IsConstantFalseWithVar) {
    LinearForm f("x");
    EXPECT_FALSE(f.is_constant());
}

TEST(LinearForm, Addition) {
    LinearForm a("x", 2.0);
    LinearForm b("x", 3.0);
    LinearForm c = a + b;
    EXPECT_DOUBLE_EQ(c.get_coeff("x"), 5.0);
}

TEST(LinearForm, AdditionMergesConstants) {
    LinearForm a(2.0);
    LinearForm b(3.0);
    LinearForm c = a + b;
    EXPECT_DOUBLE_EQ(c.constant, 5.0);
}

TEST(LinearForm, AdditionMixedTerms) {
    LinearForm a("x", 1.0);
    a.constant = 2.0;
    LinearForm b("y", 1.0);
    b.constant = 3.0;
    LinearForm c = a + b;
    EXPECT_DOUBLE_EQ(c.get_coeff("x"), 1.0);
    EXPECT_DOUBLE_EQ(c.get_coeff("y"), 1.0);
    EXPECT_DOUBLE_EQ(c.constant, 5.0);
}

TEST(LinearForm, Subtraction) {
    LinearForm a("x", 5.0);
    LinearForm b("x", 2.0);
    LinearForm c = a - b;
    EXPECT_DOUBLE_EQ(c.get_coeff("x"), 3.0);
}

TEST(LinearForm, SubtractionToZero) {
    LinearForm a("x");
    LinearForm b("x");
    LinearForm c = a - b;
    EXPECT_DOUBLE_EQ(c.get_coeff("x"), 0.0);
}

TEST(LinearForm, ScalarMultiplication) {
    LinearForm a("x", 3.0);
    a.constant = 2.0;
    LinearForm b = a * 2.0;
    EXPECT_DOUBLE_EQ(b.get_coeff("x"), 6.0);
    EXPECT_DOUBLE_EQ(b.constant, 4.0);
}

TEST(LinearForm, Negation) {
    LinearForm a("x", 3.0);
    a.constant = 1.0;
    LinearForm b = -a;
    EXPECT_DOUBLE_EQ(b.get_coeff("x"), -3.0);
    EXPECT_DOUBLE_EQ(b.constant, -1.0);
}

TEST(LinearForm, SimplifyPrunesNearZero) {
    LinearForm f;
    f.coeffs["x"] = 1e-15;
    f.constant = 1e-15;
    f.simplify();
    EXPECT_TRUE(f.coeffs.empty());
    EXPECT_DOUBLE_EQ(f.constant, 0.0);
}

TEST(LinearForm, SimplifyKeepsSignificant) {
    LinearForm f("x", 2.0);
    f.constant = 3.0;
    f.simplify();
    EXPECT_DOUBLE_EQ(f.get_coeff("x"), 2.0);
    EXPECT_DOUBLE_EQ(f.constant, 3.0);
}

// ============================================================
// LinearCollector
// ============================================================

// ─── helpers ────────────────────────────────────────────────────────────────

static ExprPtr num(double v) { return std::make_unique<Number>(v); }
static ExprPtr var(const std::string& n) { return std::make_unique<Variable>(n); }
static ExprPtr binop(ExprPtr l, ExprPtr r, BinaryOpType op) {
    return std::make_unique<BinaryOp>(std::move(l), std::move(r), op);
}

static LinearForm collect_ok(const Expr& e) {
    LinearCollector lc;
    auto r = lc.collect(e);
    EXPECT_TRUE(r.ok()) << "unexpected error in collect";
    return r.ok() ? *r : LinearForm();
}

static void collect_fail(const Expr& e) {
    LinearCollector lc;
    auto r = lc.collect(e);
    EXPECT_FALSE(r.ok()) << "expected collect to fail";
}

// ─── number ─────────────────────────────────────────────────────────────────

TEST(LinearCollector, Number) {
    auto e = num(7.0);
    LinearForm f = collect_ok(*e);
    EXPECT_TRUE(f.is_constant());
    EXPECT_DOUBLE_EQ(f.constant, 7.0);
}

TEST(LinearCollector, Zero) {
    auto e = num(0.0);
    LinearForm f = collect_ok(*e);
    EXPECT_DOUBLE_EQ(f.constant, 0.0);
}

// ─── variable ───────────────────────────────────────────────────────────────

TEST(LinearCollector, Variable) {
    auto e = var("x");
    LinearForm f = collect_ok(*e);
    EXPECT_FALSE(f.is_constant());
    EXPECT_DOUBLE_EQ(f.get_coeff("x"), 1.0);
}

// ─── addition / subtraction ──────────────────────────────────────────────────

TEST(LinearCollector, AddVarAndConst) {
    auto e = binop(var("x"), num(3.0), BinaryOpType::Add);
    LinearForm f = collect_ok(*e);
    EXPECT_DOUBLE_EQ(f.get_coeff("x"), 1.0);
    EXPECT_DOUBLE_EQ(f.constant, 3.0);
}

TEST(LinearCollector, SubVarAndConst) {
    auto e = binop(var("x"), num(2.0), BinaryOpType::Sub);
    LinearForm f = collect_ok(*e);
    EXPECT_DOUBLE_EQ(f.get_coeff("x"), 1.0);
    EXPECT_DOUBLE_EQ(f.constant, -2.0);
}

TEST(LinearCollector, AddTwoVars) {
    auto e = binop(var("x"), var("y"), BinaryOpType::Add);
    LinearForm f = collect_ok(*e);
    EXPECT_DOUBLE_EQ(f.get_coeff("x"), 1.0);
    EXPECT_DOUBLE_EQ(f.get_coeff("y"), 1.0);
}

// ─── multiplication ──────────────────────────────────────────────────────────

TEST(LinearCollector, MulConstantVar) {
    // 3 * x
    auto e = binop(num(3.0), var("x"), BinaryOpType::Mul);
    LinearForm f = collect_ok(*e);
    EXPECT_DOUBLE_EQ(f.get_coeff("x"), 3.0);
}

TEST(LinearCollector, MulVarConstant) {
    // x * 2
    auto e = binop(var("x"), num(2.0), BinaryOpType::Mul);
    LinearForm f = collect_ok(*e);
    EXPECT_DOUBLE_EQ(f.get_coeff("x"), 2.0);
}

TEST(LinearCollector, MulConstantByConstant) {
    // 3 * 4 = 12
    auto e = binop(num(3.0), num(4.0), BinaryOpType::Mul);
    LinearForm f = collect_ok(*e);
    EXPECT_DOUBLE_EQ(f.constant, 12.0);
}

TEST(LinearCollector, MulVarByVar_Fails) {
    auto e = binop(var("x"), var("y"), BinaryOpType::Mul);
    collect_fail(*e);
}

TEST(LinearCollector, MulVarBySelf_Fails) {
    auto e = binop(var("x"), var("x"), BinaryOpType::Mul);
    collect_fail(*e);
}

// ─── division ───────────────────────────────────────────────────────────────

TEST(LinearCollector, DivVarByConst) {
    // x / 4
    auto e = binop(var("x"), num(4.0), BinaryOpType::Div);
    LinearForm f = collect_ok(*e);
    EXPECT_DOUBLE_EQ(f.get_coeff("x"), 0.25);
}

TEST(LinearCollector, DivByZero_Fails) {
    auto e = binop(var("x"), num(0.0), BinaryOpType::Div);
    collect_fail(*e);
}

TEST(LinearCollector, DivByVar_Fails) {
    auto e = binop(num(1.0), var("x"), BinaryOpType::Div);
    collect_fail(*e);
}

// ─── exponentiation ─────────────────────────────────────────────────────────

TEST(LinearCollector, PowVarToOne) {
    // x^1 — linear
    auto e = binop(var("x"), num(1.0), BinaryOpType::Pow);
    LinearForm f = collect_ok(*e);
    EXPECT_DOUBLE_EQ(f.get_coeff("x"), 1.0);
}

TEST(LinearCollector, PowVarToZero) {
    // x^0 = 1 — constant
    auto e = binop(var("x"), num(0.0), BinaryOpType::Pow);
    LinearForm f = collect_ok(*e);
    EXPECT_DOUBLE_EQ(f.constant, 1.0);
    EXPECT_TRUE(f.is_constant());
}

TEST(LinearCollector, PowVarToTwo_Fails) {
    auto e = binop(var("x"), num(2.0), BinaryOpType::Pow);
    collect_fail(*e);
}

TEST(LinearCollector, PowConstToTwo_Ok) {
    // 3^2 = 9
    auto e = binop(num(3.0), num(2.0), BinaryOpType::Pow);
    LinearForm f = collect_ok(*e);
    EXPECT_DOUBLE_EQ(f.constant, 9.0);
}

// ─── unary negation ──────────────────────────────────────────────────────────

TEST(LinearCollector, UnaryNegVar) {
    auto e = std::make_unique<UnaryOp>(var("x"), UnaryOpType::Neg);
    LinearForm f = collect_ok(*e);
    EXPECT_DOUBLE_EQ(f.get_coeff("x"), -1.0);
}

TEST(LinearCollector, UnaryNegConst) {
    auto e = std::make_unique<UnaryOp>(num(5.0), UnaryOpType::Neg);
    LinearForm f = collect_ok(*e);
    EXPECT_DOUBLE_EQ(f.constant, -5.0);
}

// ─── function call ───────────────────────────────────────────────────────────

TEST(LinearCollector, FunctionOfConstant_Ok) {
    // sin(0) = 0 — constant argument is fine
    auto e = std::make_unique<FunctionCall>("sin", FuncKind::Sin, num(0.0));
    LinearForm f = collect_ok(*e);
    EXPECT_TRUE(f.is_constant());
    EXPECT_NEAR(f.constant, 0.0, 1e-9);
}

TEST(LinearCollector, FunctionOfVariable_Fails) {
    auto e = std::make_unique<FunctionCall>("sin", FuncKind::Sin, var("x"));
    collect_fail(*e);
}

// ─── array ───────────────────────────────────────────────────────────────────

TEST(LinearCollector, ArrayExpr_Fails) {
    std::vector<ExprPtr> elems;
    elems.push_back(num(1.0));
    auto e = std::make_unique<ArrayExpr>(std::move(elems));
    collect_fail(*e);
}

// ─── reuse ───────────────────────────────────────────────────────────────────

TEST(LinearCollector, ReuseCollectorTwice) {
    LinearCollector lc;

    auto e1 = var("x");
    auto r1 = lc.collect(*e1);
    EXPECT_TRUE(r1.ok());
    EXPECT_DOUBLE_EQ((*r1).get_coeff("x"), 1.0);

    auto e2 = num(5.0);
    auto r2 = lc.collect(*e2);
    EXPECT_TRUE(r2.ok());
    EXPECT_DOUBLE_EQ((*r2).constant, 5.0);
}
