// ============================================================
// Unit tests สำหรับ LinearCollector + LinearForm
// ครอบคลุม: LinearForm operators (+, -, *, negate),
//           get_coeff, variables, is_constant, simplify,
//           LinearCollector — collect จาก Number, Variable,
//           BinaryOp (ทุก op), error cases (non-linear)
// ============================================================

#include "ast/binary.h"
#include "ast/number.h"
#include "ast/variable.h"
#include "common/error.h"
#include "eval/context.h"
#include "parser/parser.h"
#include "solve/linear_collector.h"
#include <cmath>
#include <gtest/gtest.h>


using namespace math_solver;
using namespace std;

// ============================================================
// LinearForm — constructors
// ============================================================

// ทดสอบ default constructor
TEST(LinearFormTest, DefaultConstructor) {
    LinearForm f;
    EXPECT_DOUBLE_EQ(f.constant, 0.0);
    EXPECT_TRUE(f.coeffs.empty());
}

// ทดสอบ constructor จาก constant
TEST(LinearFormTest, ConstantConstructor) {
    LinearForm f(5.0);
    EXPECT_DOUBLE_EQ(f.constant, 5.0);
    EXPECT_TRUE(f.is_constant());
}

// ทดสอบ constructor จาก variable
TEST(LinearFormTest, VariableConstructor) {
    LinearForm f("x", 3.0);
    EXPECT_DOUBLE_EQ(f.get_coeff("x"), 3.0);
    EXPECT_DOUBLE_EQ(f.constant, 0.0);
}

// ทดสอบ variable constructor โดยไม่ระบุ coeff (default = 1)
TEST(LinearFormTest, VariableDefaultCoeff) {
    LinearForm f("y");
    EXPECT_DOUBLE_EQ(f.get_coeff("y"), 1.0);
}

// ============================================================
// LinearForm — get_coeff
// ============================================================

// ทดสอบ get_coeff ของ variable ที่ไม่อยู่ — ต้องได้ 0
TEST(LinearFormTest, GetCoeffMissing) {
    LinearForm f("x", 2.0);
    EXPECT_DOUBLE_EQ(f.get_coeff("y"), 0.0);
}

// ============================================================
// LinearForm — variables()
// ============================================================

// ทดสอบ variables() return เฉพาะตัวที่ coeff != 0
TEST(LinearFormTest, VariablesNonZero) {
    LinearForm f;
    f.coeffs["x"] = 1.0;
    f.coeffs["y"] = 0.0; // ค่า near-zero
    f.coeffs["z"] = -2.0;

    auto vars     = f.variables();
    EXPECT_EQ(vars.size(), 2u);
    EXPECT_EQ(vars.count("x"), 1u);
    EXPECT_EQ(vars.count("z"), 1u);
    EXPECT_EQ(vars.count("y"), 0u); // ค่า 0 ไม่ถูกรวม
}

// ทดสอบ variables() เมื่อเป็น constant form
TEST(LinearFormTest, VariablesConstant) {
    LinearForm f(10.0);
    EXPECT_TRUE(f.variables().empty());
}

// ============================================================
// LinearForm — is_constant()
// ============================================================

// ทดสอบ is_constant = true สำหรับ constant only
TEST(LinearFormTest, IsConstantTrue) {
    LinearForm f(7.0);
    EXPECT_TRUE(f.is_constant());
}

// ทดสอบ is_constant = false เมื่อมี variable
TEST(LinearFormTest, IsConstantFalse) {
    LinearForm f("x", 1.0);
    EXPECT_FALSE(f.is_constant());
}

// ============================================================
// LinearForm — operator+
// ============================================================

// ทดสอบ บวกสอง linear form
TEST(LinearFormTest, Addition) {
    LinearForm a("x", 2.0);
    a.constant = 1.0;

    LinearForm b("x", 3.0);
    b.constant        = 4.0;

    LinearForm result = a + b;
    EXPECT_DOUBLE_EQ(result.get_coeff("x"), 5.0);
    EXPECT_DOUBLE_EQ(result.constant, 5.0);
}

// ทดสอบ บวก form ที่มี variable ต่างกัน
TEST(LinearFormTest, AdditionDifferentVars) {
    LinearForm a("x", 1.0);
    LinearForm b("y", 2.0);

    LinearForm result = a + b;
    EXPECT_DOUBLE_EQ(result.get_coeff("x"), 1.0);
    EXPECT_DOUBLE_EQ(result.get_coeff("y"), 2.0);
}

// ============================================================
// LinearForm — operator-
// ============================================================

// ทดสอบ ลบสอง linear form
TEST(LinearFormTest, Subtraction) {
    LinearForm a("x", 5.0);
    a.constant = 10.0;

    LinearForm b("x", 2.0);
    b.constant        = 3.0;

    LinearForm result = a - b;
    EXPECT_DOUBLE_EQ(result.get_coeff("x"), 3.0);
    EXPECT_DOUBLE_EQ(result.constant, 7.0);
}

// ============================================================
// LinearForm — operator* (scalar)
// ============================================================

// ทดสอบ คูณด้วย scalar
TEST(LinearFormTest, ScalarMultiply) {
    LinearForm f("x", 3.0);
    f.constant        = 2.0;

    LinearForm result = f * 4.0;
    EXPECT_DOUBLE_EQ(result.get_coeff("x"), 12.0);
    EXPECT_DOUBLE_EQ(result.constant, 8.0);
}

// ทดสอบ คูณด้วย 0
TEST(LinearFormTest, ScalarMultiplyZero) {
    LinearForm f("x", 5.0);
    f.constant        = 7.0;

    LinearForm result = f * 0.0;
    EXPECT_NEAR(result.get_coeff("x"), 0.0, 1e-12);
    EXPECT_NEAR(result.constant, 0.0, 1e-12);
}

// ============================================================
// LinearForm — unary negate
// ============================================================

// ทดสอบ negate
TEST(LinearFormTest, Negate) {
    LinearForm f("x", 3.0);
    f.constant        = -2.0;

    LinearForm result = -f;
    EXPECT_DOUBLE_EQ(result.get_coeff("x"), -3.0);
    EXPECT_DOUBLE_EQ(result.constant, 2.0);
}

// ============================================================
// LinearForm — simplify()
// ============================================================

// ทดสอบ simplify ลบ near-zero coefficients
TEST(LinearFormTest, Simplify) {
    LinearForm f;
    f.coeffs["x"] = 1e-15; // near-zero → ถูกลบ
    f.coeffs["y"] = 2.0;
    f.constant    = 1e-14; // near-zero → ถูกเซ็ตเป็น 0

    f.simplify();
    EXPECT_EQ(f.coeffs.count("x"), 0u);
    EXPECT_DOUBLE_EQ(f.get_coeff("y"), 2.0);
    EXPECT_DOUBLE_EQ(f.constant, 0.0);
}

// ============================================================
// LinearCollector — collect from simple AST nodes
// ============================================================

// Helper: parse expression แล้ว collect
static LinearForm collect_expr(const string&  input,
                               const Context* ctx      = nullptr,
                               bool           isolated = false) {
    Parser          parser(input);
    auto            expr = parser.parse();
    LinearCollector collector(ctx, input, isolated);
    return collector.collect(*expr);
}

// ทดสอบ collect จากตัวเลข
TEST(LinearCollectorTest, Number) {
    auto f = collect_expr("42");
    EXPECT_TRUE(f.is_constant());
    EXPECT_DOUBLE_EQ(f.constant, 42.0);
}

// ทดสอบ collect จากตัวแปร
TEST(LinearCollectorTest, Variable) {
    auto f = collect_expr("x");
    EXPECT_DOUBLE_EQ(f.get_coeff("x"), 1.0);
    EXPECT_DOUBLE_EQ(f.constant, 0.0);
}

// ============================================================
// LinearCollector — collect from BinaryOp
// ============================================================

// ทดสอบ collect จาก Add: x + 3
TEST(LinearCollectorTest, Add) {
    auto f = collect_expr("x + 3");
    EXPECT_DOUBLE_EQ(f.get_coeff("x"), 1.0);
    EXPECT_DOUBLE_EQ(f.constant, 3.0);
}

// ทดสอบ collect จาก Sub: x - 5
TEST(LinearCollectorTest, Sub) {
    auto f = collect_expr("x - 5");
    EXPECT_DOUBLE_EQ(f.get_coeff("x"), 1.0);
    EXPECT_DOUBLE_EQ(f.constant, -5.0);
}

// ทดสอบ collect จาก Mul: 3 * x (constant * variable)
TEST(LinearCollectorTest, MulConstLeft) {
    auto f = collect_expr("3 * x");
    EXPECT_DOUBLE_EQ(f.get_coeff("x"), 3.0);
}

// ทดสอบ collect จาก Mul: x * 3 (variable * constant)
TEST(LinearCollectorTest, MulConstRight) {
    auto f = collect_expr("x * 3");
    EXPECT_DOUBLE_EQ(f.get_coeff("x"), 3.0);
}

// ทดสอบ collect จาก Div: x / 2
TEST(LinearCollectorTest, DivByConstant) {
    auto f = collect_expr("x / 2");
    EXPECT_DOUBLE_EQ(f.get_coeff("x"), 0.5);
}

// ทดสอบ collect จาก Pow: x^1 (ต้อง linear)
TEST(LinearCollectorTest, PowOne) {
    auto f = collect_expr("x ^ 1");
    EXPECT_DOUBLE_EQ(f.get_coeff("x"), 1.0);
}

// ทดสอบ collect จาก Pow: x^0 = 1 (constant)
TEST(LinearCollectorTest, PowZero) {
    auto f = collect_expr("x ^ 0");
    EXPECT_TRUE(f.is_constant());
    EXPECT_DOUBLE_EQ(f.constant, 1.0);
}

// ทดสอบ collect จาก Pow: 2^3 = 8 (constant^constant)
TEST(LinearCollectorTest, PowConstant) {
    auto f = collect_expr("2 ^ 3");
    EXPECT_TRUE(f.is_constant());
    EXPECT_DOUBLE_EQ(f.constant, 8.0);
}

// ============================================================
// LinearCollector — complex linear expression
// ============================================================

// ทดสอบ 2x + 3y - 5
TEST(LinearCollectorTest, MultiVariable) {
    auto f = collect_expr("2x + 3y - 5");
    EXPECT_DOUBLE_EQ(f.get_coeff("x"), 2.0);
    EXPECT_DOUBLE_EQ(f.get_coeff("y"), 3.0);
    EXPECT_DOUBLE_EQ(f.constant, -5.0);
}

// ============================================================
// LinearCollector — with context (substitution)
// ============================================================

// ทดสอบ substitute variable จาก context
TEST(LinearCollectorTest, SubstituteFromContext) {
    Context ctx;
    ctx.set("a", 4);
    auto f = collect_expr("a * x + 1", &ctx);
    EXPECT_DOUBLE_EQ(f.get_coeff("x"), 4.0);
    EXPECT_DOUBLE_EQ(f.constant, 1.0);
}

// ทดสอบ isolated mode — ไม่ substitute
TEST(LinearCollectorTest, IsolatedMode) {
    Context ctx;
    ctx.set("x", 10);
    auto f = collect_expr("x + 1", &ctx, true);
    // x ต้องยังคงเป็น variable
    EXPECT_DOUBLE_EQ(f.get_coeff("x"), 1.0);
    EXPECT_DOUBLE_EQ(f.constant, 1.0);
}

// ============================================================
// LinearCollector — error cases (non-linear)
// ============================================================

// ทดสอบ non-linear: x * y
TEST(LinearCollectorTest, NonLinearMul) {
    EXPECT_THROW(collect_expr("x * y"), NonLinearError);
}

// ทดสอบ non-linear: x / y
TEST(LinearCollectorTest, NonLinearDiv) {
    EXPECT_THROW(collect_expr("x / y"), NonLinearError);
}

// ทดสอบ non-linear: x ^ 2
TEST(LinearCollectorTest, NonLinearPow) {
    EXPECT_THROW(collect_expr("x ^ 2"), NonLinearError);
}

// ทดสอบ non-linear: x ^ y (variable exponent)
TEST(LinearCollectorTest, VariableExponent) {
    EXPECT_THROW(collect_expr("2 ^ x"), NonLinearError);
}

// ทดสอบ division by zero
TEST(LinearCollectorTest, DivisionByZero) {
    EXPECT_THROW(collect_expr("x / 0"), MathError);
}

// ============================================================
// LinearCollector — set_input / set_isolated
// ============================================================

// ทดสอบ set_input
TEST(LinearCollectorTest, SetInput) {
    LinearCollector collector;
    collector.set_input("test");
    // ไม่ throw — แค่ set internal state
    Number num(5);
    auto   f = collector.collect(num);
    EXPECT_DOUBLE_EQ(f.constant, 5.0);
}

// ทดสอบ set_isolated
TEST(LinearCollectorTest, SetIsolated) {
    Context ctx;
    ctx.set("x", 10);
    LinearCollector collector(&ctx, "x", false);
    collector.set_isolated(true);

    Variable var("x");
    auto     f = collector.collect(var);
    EXPECT_DOUBLE_EQ(f.get_coeff("x"), 1.0); // ไม่ substitute
}
