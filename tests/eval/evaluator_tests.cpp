// ============================================================
// Unit tests สำหรับ Evaluator
// ครอบคลุม: evaluate ของ Number, Variable, BinaryOp (ทุก op),
//           division by zero, undefined variable, context usage
// ============================================================

#include "ast/binary.h"
#include "ast/number.h"
#include "ast/variable.h"
#include "common/error.h"
#include "eval/context.h"
#include "eval/evaluator.h"
#include "parser/parser.h"
#include <cmath>
#include <gtest/gtest.h>


using namespace math_solver;
using namespace std;

// ============================================================
// Helper: parse แล้ว evaluate (สะดวก)
// ============================================================
static double eval(const string& input, const Context* ctx = nullptr) {
    Parser    parser(input);
    auto      expr = parser.parse();
    Evaluator evaluator(ctx, input);
    return evaluator.evaluate(*expr);
}

// ============================================================
// Number evaluation
// ============================================================

// ทดสอบ evaluate ตัวเลข
TEST(EvaluatorTest, Number) { EXPECT_DOUBLE_EQ(eval("42"), 42.0); }

// ทดสอบ evaluate ทศนิยม
TEST(EvaluatorTest, Decimal) { EXPECT_DOUBLE_EQ(eval("3.14"), 3.14); }

// ทดสอบ evaluate ค่า 0
TEST(EvaluatorTest, Zero) { EXPECT_DOUBLE_EQ(eval("0"), 0.0); }

// ============================================================
// Arithmetic operations
// ============================================================

// ทดสอบ บวก
TEST(EvaluatorTest, Addition) { EXPECT_DOUBLE_EQ(eval("1 + 2"), 3.0); }

// ทดสอบ ลบ
TEST(EvaluatorTest, Subtraction) { EXPECT_DOUBLE_EQ(eval("10 - 4"), 6.0); }

// ทดสอบ คูณ
TEST(EvaluatorTest, Multiplication) { EXPECT_DOUBLE_EQ(eval("3 * 7"), 21.0); }

// ทดสอบ หาร
TEST(EvaluatorTest, Division) { EXPECT_DOUBLE_EQ(eval("15 / 3"), 5.0); }

// ทดสอบ ยกกำลัง
TEST(EvaluatorTest, Power) { EXPECT_DOUBLE_EQ(eval("2 ^ 10"), 1024.0); }

// ทดสอบ ยกกำลัง 0 (ผลต้องเป็น 1)
TEST(EvaluatorTest, PowerZero) { EXPECT_DOUBLE_EQ(eval("5 ^ 0"), 1.0); }

// ============================================================
// Precedence / complex expressions
// ============================================================

// ทดสอบ precedence: 2 + 3 * 4 = 14
TEST(EvaluatorTest, Precedence) { EXPECT_DOUBLE_EQ(eval("2 + 3 * 4"), 14.0); }

// ทดสอบ parentheses: (2 + 3) * 4 = 20
TEST(EvaluatorTest, Parentheses) {
    EXPECT_DOUBLE_EQ(eval("(2 + 3) * 4"), 20.0);
}

// ทดสอบ nested: ((1 + 2) * (3 + 4)) = 21
TEST(EvaluatorTest, NestedExpr) {
    EXPECT_DOUBLE_EQ(eval("(1 + 2) * (3 + 4)"), 21.0);
}

// ทดสอบ unary minus: -5 = -5
TEST(EvaluatorTest, UnaryMinus) { EXPECT_DOUBLE_EQ(eval("-5"), -5.0); }

// ทดสอบ double unary: --3 = 3
TEST(EvaluatorTest, DoubleUnaryMinus) { EXPECT_DOUBLE_EQ(eval("--3"), 3.0); }

// ทดสอบ implicit multiplication: "2(3)" = 6
TEST(EvaluatorTest, ImplicitMul) { EXPECT_DOUBLE_EQ(eval("2(3)"), 6.0); }

// ============================================================
// Variable evaluation
// ============================================================

// ทดสอบ evaluate ตัวแปรที่กำหนดค่าแล้ว
TEST(EvaluatorTest, VariableFromContext) {
    Context ctx;
    ctx.set("x", 7);
    EXPECT_DOUBLE_EQ(eval("x", &ctx), 7.0);
}

// ทดสอบ evaluate expression ที่มีตัวแปร: "x + 3"
TEST(EvaluatorTest, VariableInExpression) {
    Context ctx;
    ctx.set("x", 10);
    EXPECT_DOUBLE_EQ(eval("x + 3", &ctx), 13.0);
}

// ทดสอบ evaluate ตัวแปรหลายตัว
TEST(EvaluatorTest, MultipleVariables) {
    Context ctx;
    ctx.set("a", 2);
    ctx.set("b", 3);
    EXPECT_DOUBLE_EQ(eval("a * b + 1", &ctx), 7.0);
}

// ทดสอบ implicit multiplication กับตัวแปร: "2x" where x=5
TEST(EvaluatorTest, ImplicitMulVariable) {
    Context ctx;
    ctx.set("x", 5);
    EXPECT_DOUBLE_EQ(eval("2x", &ctx), 10.0);
}

// ============================================================
// Error cases
// ============================================================

// ทดสอบ division by zero
TEST(EvaluatorTest, DivisionByZero) { EXPECT_THROW(eval("1 / 0"), MathError); }

// ทดสอบ undefined variable (ไม่มี context)
TEST(EvaluatorTest, UndefinedVariableNoContext) {
    EXPECT_THROW(eval("x"), UndefinedVariableError);
}

// ทดสอบ undefined variable (มี context แต่ไม่มีตัวแปรนั้น)
TEST(EvaluatorTest, UndefinedVariableNotInContext) {
    Context ctx;
    ctx.set("y", 1);
    EXPECT_THROW(eval("x", &ctx), UndefinedVariableError);
}

// ============================================================
// Evaluator constructors / set_input
// ============================================================

// ทดสอบ default constructor
TEST(EvaluatorTest, DefaultConstructor) {
    Evaluator evaluator;
    Number    num(99);
    EXPECT_DOUBLE_EQ(evaluator.evaluate(num), 99.0);
}

// ทดสอบ set_input เพื่อให้ error message มี context
TEST(EvaluatorTest, SetInput) {
    Evaluator evaluator;
    evaluator.set_input("test input");
    Number num(1);
    // set_input ไม่เปลี่ยนพฤติกรรมการ evaluate แค่เก็บ input ไว้ใช้ในข้อความ error
    EXPECT_DOUBLE_EQ(evaluator.evaluate(num), 1.0);
}

// ============================================================
// Edge cases
// ============================================================

// ทดสอบ expression ยาว: 1 + 2 + 3 + 4 + 5 = 15
TEST(EvaluatorTest, LongAddition) {
    EXPECT_DOUBLE_EQ(eval("1 + 2 + 3 + 4 + 5"), 15.0);
}

// ทดสอบ negative result: 3 - 10 = -7
TEST(EvaluatorTest, NegativeResult) { EXPECT_DOUBLE_EQ(eval("3 - 10"), -7.0); }

// ทดสอบ fractional division: 7 / 2 = 3.5
TEST(EvaluatorTest, FractionalDivision) {
    EXPECT_DOUBLE_EQ(eval("7 / 2"), 3.5);
}

// ทดสอบ power with fraction: 4 ^ 0.5 = 2 (square root)
TEST(EvaluatorTest, SquareRoot) { EXPECT_DOUBLE_EQ(eval("4 ^ 0.5"), 2.0); }
