#include <gtest/gtest.h>

#include "algebra/linear/simplify.hpp"
#include "ast/math/binary_expr.hpp"
#include "ast/math/equation_expr.hpp"
#include "ast/math/number_expr.hpp"
#include "ast/math/variable_expr.hpp"

using namespace math_solver;

// ─── helpers ────────────────────────────────────────────────────────────────

static ExprPtr num(double v) { return std::make_unique<Number>(v); }
static ExprPtr var(const std::string& n) { return std::make_unique<Variable>(n); }
static ExprPtr binop(ExprPtr l, ExprPtr r, BinaryOpType op) {
    return std::make_unique<BinaryOp>(std::move(l), std::move(r), op);
}

// Makes a simple equation LHS = RHS
static Equation make_eq(ExprPtr lhs, ExprPtr rhs) {
    return Equation(std::move(lhs), std::move(rhs));
}

// ─── simplify (equation) ────────────────────────────────────────────────────

TEST(Simplify, SingleVarEqualsConst) {
    // x = 5 → "x = 5"
    Simplifier s;
    Equation eq = make_eq(var("x"), num(5.0));
    auto r = s.simplify(eq);
    EXPECT_EQ(r.canonical, "x = 5");
}

TEST(Simplify, ConstEqualsVar) {
    // 5 = x → move to LHS → "-x = -5" normalized or "x = 5"
    // The normalizer puts variable terms on LHS with RHS as constant.
    Simplifier s;
    Equation eq = make_eq(num(5.0), var("x"));
    auto r = s.simplify(eq);
    // canonical: normalized form should show "-x = -5" or rearranged
    EXPECT_FALSE(r.canonical.empty());
}

TEST(Simplify, TwoVars) {
    // x + y = 3
    Simplifier s;
    auto lhs = binop(var("x"), var("y"), BinaryOpType::Add);
    Equation eq = make_eq(std::move(lhs), num(3.0));
    auto r = s.simplify(eq);
    EXPECT_NE(r.canonical.find("x"), std::string::npos);
    EXPECT_NE(r.canonical.find("y"), std::string::npos);
    EXPECT_NE(r.canonical.find("3"), std::string::npos);
}

TEST(Simplify, TermsMovedToLHS) {
    // 2x + 3 = x + 7 → x = 4
    Simplifier s;
    auto lhs = binop(binop(num(2.0), var("x"), BinaryOpType::Mul),
                     num(3.0), BinaryOpType::Add);
    auto rhs = binop(var("x"), num(7.0), BinaryOpType::Add);
    Equation eq = make_eq(std::move(lhs), std::move(rhs));
    auto r = s.simplify(eq);
    // LHS - RHS = x - 4, so canonical: "x = 4"
    EXPECT_EQ(r.canonical, "x = 4");
}

TEST(Simplify, TautologyDetected) {
    // x = x → 0 = 0 → infinite solutions
    Simplifier s;
    Equation eq = make_eq(var("x"), var("x"));
    auto r = s.simplify(eq);
    EXPECT_TRUE(r.is_infinite_solutions());
}

TEST(Simplify, NoSolutionDetected) {
    // 1 = 2 → -1 = 0 → no solution
    Simplifier s;
    Equation eq = make_eq(num(1.0), num(2.0));
    auto r = s.simplify(eq);
    EXPECT_TRUE(r.is_no_solution());
}

TEST(Simplify, NonLinearEquationFails) {
    // x * x = 1 — non-linear, simplify should set warning and empty canonical
    Simplifier s;
    auto lhs = binop(var("x"), var("x"), BinaryOpType::Mul);
    Equation eq = make_eq(std::move(lhs), num(1.0));
    auto r = s.simplify(eq);
    EXPECT_TRUE(r.canonical.empty());
    EXPECT_FALSE(r.warnings.empty());
}

TEST(Simplify, CustomVarOrder) {
    // y + x = 5 with var_order = ["x", "y"]
    Simplifier s;
    auto lhs = binop(var("y"), var("x"), BinaryOpType::Add);
    Equation eq = make_eq(std::move(lhs), num(5.0));
    SimplifyOptions opts;
    opts.var_order = {"x", "y"};
    auto r = s.simplify(eq, opts);
    // x should appear before y in canonical
    size_t px = r.canonical.find("x");
    size_t py = r.canonical.find("y");
    EXPECT_NE(px, std::string::npos);
    EXPECT_NE(py, std::string::npos);
    EXPECT_LT(px, py);
}

// ─── simplify_expr (expression) ─────────────────────────────────────────────

TEST(SimplifyExpr, SingleVar) {
    // x
    Simplifier s;
    auto e = var("x");
    auto r = s.simplify_expr(*e);
    EXPECT_EQ(r.canonical, "x");
}

TEST(SimplifyExpr, LinearExpression) {
    // 2x + 3
    Simplifier s;
    auto e = binop(binop(num(2.0), var("x"), BinaryOpType::Mul),
                   num(3.0), BinaryOpType::Add);
    auto r = s.simplify_expr(*e);
    EXPECT_NE(r.canonical.find("2x"), std::string::npos);
    EXPECT_NE(r.canonical.find("3"), std::string::npos);
}

TEST(SimplifyExpr, ConstantExpression) {
    // 4 + 5 = 9
    Simplifier s;
    auto e = binop(num(4.0), num(5.0), BinaryOpType::Add);
    auto r = s.simplify_expr(*e);
    EXPECT_EQ(r.canonical, "9");
}

TEST(SimplifyExpr, NonLinearFails) {
    // x * x — non-linear
    Simplifier s;
    auto e = binop(var("x"), var("x"), BinaryOpType::Mul);
    auto r = s.simplify_expr(*e);
    EXPECT_TRUE(r.canonical.empty());
    EXPECT_FALSE(r.warnings.empty());
}

// ─── SimplifyResult predicates ───────────────────────────────────────────────

TEST(SimplifyResult, IsNoSolution) {
    SimplifyResult r;
    r.form = LinearForm(5.0); // constant ≠ 0
    EXPECT_TRUE(r.is_no_solution());
}

TEST(SimplifyResult, IsInfiniteSolutions) {
    SimplifyResult r;
    r.form = LinearForm(0.0); // constant = 0, no variables
    EXPECT_TRUE(r.is_infinite_solutions());
}

TEST(SimplifyResult, IsNotNoSolutionWithVar) {
    SimplifyResult r;
    r.form = LinearForm("x");
    EXPECT_FALSE(r.is_no_solution());
    EXPECT_FALSE(r.is_infinite_solutions());
}
