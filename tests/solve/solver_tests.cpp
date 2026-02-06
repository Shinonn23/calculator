// ============================================================
// Unit tests สำหรับ EquationSolver
// ครอบคลุม: solve() — สมการเชิงเส้นตัวแปรเดียว,
//           solve_for() — solve สำหรับตัวแปรที่กำหนด,
//           error cases (no solution, infinite solutions,
//           multiple unknowns, variable not in equation),
//           SolveResult.to_string()
// ============================================================

#include "common/error.h"
#include "eval/context.h"
#include "parser/parser.h"
#include "solve/solver.h"
#include <cmath>
#include <gtest/gtest.h>

using namespace math_solver;
using namespace std;

// ============================================================
// Helper: parse equation string แล้ว solve
// ============================================================
static SolveResult solve_eq(const string& input, const Context* ctx = nullptr) {
    Parser         parser(input);
    auto           eq = parser.parse_equation();
    EquationSolver solver(ctx, input);
    return solver.solve(*eq);
}

// ============================================================
// SolveResult.to_string()
// ============================================================

// ทดสอบ to_string เมื่อมีคำตอบ
TEST(SolveResultTest, HasSolution) {
    SolveResult r;
    r.variable     = "x";
    r.values       = {5.0};
    r.has_solution = true;
    EXPECT_EQ(r.to_string(), "x = 5");
}

// ทดสอบ to_string เมื่อไม่มีคำตอบ
TEST(SolveResultTest, NoSolution) {
    SolveResult r;
    r.has_solution = false;
    EXPECT_EQ(r.to_string(), "no solution");
}

// ทดสอบ to_string กับทศนิยม (ลบ trailing zeros)
TEST(SolveResultTest, DecimalValue) {
    SolveResult r;
    r.variable     = "y";
    r.values       = {2.5};
    r.has_solution = true;
    EXPECT_EQ(r.to_string(), "y = 2.5");
}

// ============================================================
// solve() — basic linear equations
// ============================================================

// ทดสอบ x = 5 — trivial
TEST(SolverTest, TrivialEquation) {
    auto r = solve_eq("x = 5");
    EXPECT_TRUE(r.has_solution);
    EXPECT_EQ(r.variable, "x");
    EXPECT_DOUBLE_EQ(r.value(), 5.0);
}

// ทดสอบ 2x = 10 → x = 5
TEST(SolverTest, SimpleLinear) {
    auto r = solve_eq("2x = 10");
    EXPECT_TRUE(r.has_solution);
    EXPECT_DOUBLE_EQ(r.value(), 5.0);
}

// ทดสอบ x + 3 = 7 → x = 4
TEST(SolverTest, WithConstant) {
    auto r = solve_eq("x + 3 = 7");
    EXPECT_TRUE(r.has_solution);
    EXPECT_DOUBLE_EQ(r.value(), 4.0);
}

// ทดสอบ 3x + 1 = 10 → x = 3
TEST(SolverTest, CoeffAndConstant) {
    auto r = solve_eq("3x + 1 = 10");
    EXPECT_TRUE(r.has_solution);
    EXPECT_DOUBLE_EQ(r.value(), 3.0);
}

// ทดสอบ ตัวแปรทั้งสองฝั่ง: 2x = x + 4 → x = 4
TEST(SolverTest, VariableBothSides) {
    auto r = solve_eq("2x = x + 4");
    EXPECT_TRUE(r.has_solution);
    EXPECT_DOUBLE_EQ(r.value(), 4.0);
}

// ทดสอบ ค่าเป็นลบ: x + 10 = 3 → x = -7
TEST(SolverTest, NegativeResult) {
    auto r = solve_eq("x + 10 = 3");
    EXPECT_TRUE(r.has_solution);
    EXPECT_DOUBLE_EQ(r.value(), -7.0);
}

// ทดสอบ ค่าทศนิยม: 2x = 5 → x = 2.5
TEST(SolverTest, FractionalResult) {
    auto r = solve_eq("2x = 5");
    EXPECT_TRUE(r.has_solution);
    EXPECT_DOUBLE_EQ(r.value(), 2.5);
}

// ============================================================
// solve() — error cases
// ============================================================

// ทดสอบ no solution: 0x = 5 → เช่น "x - x = 5"
TEST(SolverTest, NoSolution) {
    EXPECT_THROW(solve_eq("x - x = 5"), NoSolutionError);
}

// ทดสอบ infinite solutions: x - x = 0
TEST(SolverTest, InfiniteSolutions) {
    EXPECT_THROW(solve_eq("x - x = 0"), InfiniteSolutionsError);
}

// ทดสอบ solvable constant equation: 5 = 5 → infinite
TEST(SolverTest, ConstantTrueEquation) {
    EXPECT_THROW(solve_eq("5 = 5"), InfiniteSolutionsError);
}

// ทดสอบ constant false: 3 = 5 → no solution
TEST(SolverTest, ConstantFalseEquation) {
    EXPECT_THROW(solve_eq("3 = 5"), NoSolutionError);
}

// ทดสอบ multiple unknowns: x + y = 5
TEST(SolverTest, MultipleUnknowns) {
    EXPECT_THROW(solve_eq("x + y = 5"), MultipleUnknownsError);
}

// ============================================================
// solve() — with context
// ============================================================

// ทดสอบ solve โดย substitute จาก context
TEST(SolverTest, WithContext) {
    Context ctx;
    ctx.set("a", 3);
    // a*x = 12 → 3x = 12 → x = 4
    auto r = solve_eq("a * x = 12", &ctx);
    EXPECT_TRUE(r.has_solution);
    EXPECT_DOUBLE_EQ(r.value(), 4.0);
}

// ============================================================
// solve_for()
// ============================================================

// ทดสอบ solve_for — solve สำหรับ target variable ที่ระบุ
TEST(SolverTest, SolveForBasic) {
    Context        ctx;
    // x + 3 = 10, solve for x
    Parser         parser("x + 3 = 10");
    auto           eq = parser.parse_equation();
    EquationSolver solver(nullptr, "x + 3 = 10");
    auto           r = solver.solve_for(*eq, "x");
    EXPECT_DOUBLE_EQ(r.value(), 7.0);
}

// ทดสอบ solve_for — target variable ไม่อยู่ใน equation
TEST(SolverTest, SolveForNotInEquation) {
    Parser         parser("x + 1 = 5");
    auto           eq = parser.parse_equation();
    EquationSolver solver(nullptr, "x + 1 = 5");
    EXPECT_THROW(solver.solve_for(*eq, "z"), InvalidEquationError);
}

// ============================================================
// EquationSolver constructors
// ============================================================

// ทดสอบ default constructor
TEST(SolverTest, DefaultConstructor) {
    EquationSolver solver;
    // ทำงานได้ — ไม่มี context
    Parser         parser("x = 5");
    auto           eq = parser.parse_equation();
    solver.set_input("x = 5");
    auto r = solver.solve(*eq);
    EXPECT_DOUBLE_EQ(r.value(), 5.0);
}

// ทดสอบ constructor กับ context เท่านั้น
TEST(SolverTest, ContextOnlyConstructor) {
    Context ctx;
    ctx.set("a", 2);
    EquationSolver solver(&ctx);
    solver.set_input("a * x = 8");

    Parser parser("a * x = 8");
    auto   eq = parser.parse_equation();
    auto   r  = solver.solve(*eq);
    EXPECT_DOUBLE_EQ(r.value(), 4.0);
}

// ============================================================
// Quadratic equation tests
// ============================================================

// x^2 = 4 → x = [-2, 2]
TEST(SolverTest, QuadraticSimple) {
    auto r = solve_eq("x^2 = 4");
    EXPECT_TRUE(r.has_solution);
    ASSERT_EQ(r.values.size(), 2u);
    EXPECT_NEAR(r.values[0], -2.0, 1e-10);
    EXPECT_NEAR(r.values[1], 2.0, 1e-10);
}

// x^2 - 5x + 6 = 0 → x = [2, 3]
TEST(SolverTest, QuadraticFactorable) {
    auto r = solve_eq("x^2 - 5x + 6 = 0");
    EXPECT_TRUE(r.has_solution);
    ASSERT_EQ(r.values.size(), 2u);
    EXPECT_NEAR(r.values[0], 2.0, 1e-10);
    EXPECT_NEAR(r.values[1], 3.0, 1e-10);
}

// x^2 - 4x + 4 = 0 → x = 2 (repeated root)
TEST(SolverTest, QuadraticRepeatedRoot) {
    auto r = solve_eq("x^2 - 4x + 4 = 0");
    EXPECT_TRUE(r.has_solution);
    ASSERT_EQ(r.values.size(), 1u);
    EXPECT_NEAR(r.values[0], 2.0, 1e-10);
}

// x^2 + 1 = 0 → no real solution
TEST(SolverTest, QuadraticNoRealSolution) {
    EXPECT_THROW(solve_eq("x^2 + 1 = 0"), NoSolutionError);
}

// 2x^2 + 4x - 6 = 0 → x = [-3, 1]
TEST(SolverTest, QuadraticWithCoefficients) {
    auto r = solve_eq("2x^2 + 4x - 6 = 0");
    EXPECT_TRUE(r.has_solution);
    ASSERT_EQ(r.values.size(), 2u);
    EXPECT_NEAR(r.values[0], -3.0, 1e-10);
    EXPECT_NEAR(r.values[1], 1.0, 1e-10);
}

// x^2 = 0 → x = 0
TEST(SolverTest, QuadraticZeroSolution) {
    auto r = solve_eq("x^2 = 0");
    EXPECT_TRUE(r.has_solution);
    ASSERT_EQ(r.values.size(), 1u);
    EXPECT_NEAR(r.values[0], 0.0, 1e-10);
}

// Quadratic with context: a*x^2 + b = 0 where a=1, b=-9
TEST(SolverTest, QuadraticWithContext) {
    Context ctx;
    ctx.set("a", 1);
    ctx.set("b", -9);
    auto r = solve_eq("a*x^2 + b = 0", &ctx);
    EXPECT_TRUE(r.has_solution);
    ASSERT_EQ(r.values.size(), 2u);
    EXPECT_NEAR(r.values[0], -3.0, 1e-10);
    EXPECT_NEAR(r.values[1], 3.0, 1e-10);
}

// SolveResult multi-solution to_string
TEST(SolveResultTest, MultiSolutionToString) {
    SolveResult r;
    r.variable     = "x";
    r.values       = {-2.0, 2.0};
    r.has_solution = true;
    EXPECT_EQ(r.to_string(), "x = [-2, 2]");
}
