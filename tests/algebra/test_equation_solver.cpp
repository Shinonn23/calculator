#include <gtest/gtest.h>

#include "algebra/solver/solver.hpp"
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

// ─── basic solve ─────────────────────────────────────────────────────────────

TEST(EquationSolver, XEqualsConst) {
    // x = 5 → x = 5
    EquationSolver solver;
    Equation eq = make_eq(var("x"), num(5.0));
    auto r = solver.solve(eq);
    ASSERT_TRUE(r.ok());
    EXPECT_EQ((*r).variable, "x");
    EXPECT_NEAR((*r).value, 5.0, 1e-9);
    EXPECT_TRUE((*r).has_solution);
}

TEST(EquationSolver, LinearEquation) {
    // x + 3 = 7 → x = 4
    EquationSolver solver;
    auto lhs = binop(var("x"), num(3.0), BinaryOpType::Add);
    Equation eq = make_eq(std::move(lhs), num(7.0));
    auto r = solver.solve(eq);
    ASSERT_TRUE(r.ok());
    EXPECT_NEAR((*r).value, 4.0, 1e-9);
}

TEST(EquationSolver, CoefficientEquation) {
    // 2x = 6 → x = 3
    EquationSolver solver;
    auto lhs = binop(num(2.0), var("x"), BinaryOpType::Mul);
    Equation eq = make_eq(std::move(lhs), num(6.0));
    auto r = solver.solve(eq);
    ASSERT_TRUE(r.ok());
    EXPECT_NEAR((*r).value, 3.0, 1e-9);
}

TEST(EquationSolver, NegativeCoefficient) {
    // -x = 4 → x = -4
    EquationSolver solver;
    auto lhs = binop(num(-1.0), var("x"), BinaryOpType::Mul);
    Equation eq = make_eq(std::move(lhs), num(4.0));
    auto r = solver.solve(eq);
    ASSERT_TRUE(r.ok());
    EXPECT_NEAR((*r).value, -4.0, 1e-9);
}

TEST(EquationSolver, FractionalResult) {
    // x = 1.5 (represented as 3/2)
    EquationSolver solver;
    auto lhs = binop(num(2.0), var("x"), BinaryOpType::Mul);
    Equation eq = make_eq(std::move(lhs), num(3.0));
    auto r = solver.solve(eq);
    ASSERT_TRUE(r.ok());
    EXPECT_NEAR((*r).value, 1.5, 1e-9);
}

// ─── no solution / infinite solutions ───────────────────────────────────────

TEST(EquationSolver, NoSolution) {
    // 1 = 2 → 0 = -1 → no solution
    EquationSolver solver;
    Equation eq = make_eq(num(1.0), num(2.0));
    auto r = solver.solve(eq);
    EXPECT_FALSE(r.ok());
}

TEST(EquationSolver, InfiniteSolutions) {
    // x = x → 0 = 0 → infinite solutions
    EquationSolver solver;
    Equation eq = make_eq(var("x"), var("x"));
    auto r = solver.solve(eq);
    EXPECT_FALSE(r.ok());
}

TEST(EquationSolver, ZeroEqualsZero_InfiniteSolutions) {
    // 0 = 0
    EquationSolver solver;
    Equation eq = make_eq(num(0.0), num(0.0));
    auto r = solver.solve(eq);
    EXPECT_FALSE(r.ok());
}

// ─── SolveResult::to_string ──────────────────────────────────────────────────

TEST(SolveResult, ToStringWithSolution) {
    SolveResult r{"x", 3.0, true};
    EXPECT_EQ(r.to_string(), "x = 3");
}

TEST(SolveResult, ToStringNoSolution) {
    SolveResult r{"", 0.0, false};
    EXPECT_EQ(r.to_string(), "no solution");
}

TEST(SolveResult, ToStringTrimsTrailingZeros) {
    SolveResult r{"y", 1.5, true};
    std::string s = r.to_string();
    EXPECT_EQ(s, "y = 1.5");
}

// ─── solve_for ───────────────────────────────────────────────────────────────

TEST(EquationSolver, SolveForSpecificVar) {
    // x + y = 5, solve for x → but we need y to be a constant
    // Without context, both x and y are free — should fail (multiple unknowns)
    // Instead test simple case: x = 5, solve_for "x"
    EquationSolver solver;
    Equation eq = make_eq(var("x"), num(5.0));
    auto r = solver.solve_for(eq, "x");
    ASSERT_TRUE(r.ok());
    EXPECT_EQ((*r).variable, "x");
    EXPECT_NEAR((*r).value, 5.0, 1e-9);
}

TEST(EquationSolver, SolveForVarNotPresent_Fails) {
    // x = 5, solve for "y" (not in equation)
    EquationSolver solver;
    Equation eq = make_eq(var("x"), num(5.0));
    auto r = solver.solve_for(eq, "y");
    EXPECT_FALSE(r.ok());
}
