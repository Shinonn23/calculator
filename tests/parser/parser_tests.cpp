// ============================================================
// Unit tests สำหรับ Parser
// ครอบคลุม: parse(), parse_equation(), parse_expression_or_equation()
//           และ precedence / implicit multiplication / error cases
// ============================================================

#include "ast/binary.h"
#include "ast/equation.h"
#include "ast/number.h"
#include "ast/variable.h"
#include "common/error.h"
#include "parser/parser.h"
#include <gtest/gtest.h>

using namespace math_solver;
using namespace std;

// ============================================================
// Helper: parse แล้ว return to_string (สะดวกเทียบผลลัพธ์)
// ============================================================
static string parse_str(const string& input) {
    Parser parser(input);
    auto   expr = parser.parse();
    return expr->to_string();
}

// ============================================================
// parse() — basic expression parsing
// ============================================================

// ทดสอบ parse ตัวเลขเดียว
TEST(ParserTest, SingleNumber) { EXPECT_EQ(parse_str("42"), "42"); }

// ทดสอบ parse ทศนิยม
TEST(ParserTest, DecimalNumber) { EXPECT_EQ(parse_str("3.14"), "3.14"); }

// ทดสอบ parse ตัวแปรเดียว
TEST(ParserTest, SingleVariable) { EXPECT_EQ(parse_str("x"), "x"); }

// ทดสอบ parse บวก
TEST(ParserTest, Addition) { EXPECT_EQ(parse_str("1 + 2"), "1 + 2"); }

// ทดสอบ parse ลบ
TEST(ParserTest, Subtraction) { EXPECT_EQ(parse_str("5 - 3"), "5 - 3"); }

// ทดสอบ parse คูณ
TEST(ParserTest, Multiplication) { EXPECT_EQ(parse_str("4 * 6"), "4*6"); }

// ทดสอบ parse หาร
TEST(ParserTest, Division) { EXPECT_EQ(parse_str("10 / 2"), "10/2"); }

// ทดสอบ parse ยกกำลัง
TEST(ParserTest, Power) { EXPECT_EQ(parse_str("2 ^ 3"), "2^3"); }

// ============================================================
// Operator precedence
// ============================================================

// ทดสอบ precedence: * ก่อน +
TEST(ParserPrecedenceTest, MulBeforeAdd) {
    // 1 + 2 * 3 = 1 + (2*3)
    EXPECT_EQ(parse_str("1 + 2 * 3"), "1 + 2*3");
}

// ทดสอบ precedence: ^ ก่อน *
TEST(ParserPrecedenceTest, PowBeforeMul) {
    // 2 * 3 ^ 2 = 2 * (3^2)
    EXPECT_EQ(parse_str("2 * 3 ^ 2"), "2*3^2");
}

// ทดสอบ precedence: - ก่อน + (left assoc, ลำดับเดียวกัน)
TEST(ParserPrecedenceTest, LeftAssocAddSub) {
    // 1 - 2 + 3 = ((1-2)+3)
    EXPECT_EQ(parse_str("1 - 2 + 3"), "1 - 2 + 3");
}

// ทดสอบ precedence: / ก่อน - (ไม่ใช่จริงๆ — / precedence > -)
TEST(ParserPrecedenceTest, DivBeforeSub) {
    // 6 - 4 / 2 = 6 - (4/2)
    EXPECT_EQ(parse_str("6 - 4 / 2"), "6 - 4/2");
}

// ============================================================
// Parentheses
// ============================================================

// ทดสอบ parentheses ที่เปลี่ยน precedence
TEST(ParserParenTest, OverridePrecedence) {
    // (1 + 2) * 3
    EXPECT_EQ(parse_str("(1 + 2) * 3"), "(1 + 2)*3");
}

// ทดสอบ nested parentheses
TEST(ParserParenTest, NestedParens) {
    EXPECT_EQ(parse_str("((1 + 2))"), "1 + 2");
}

// ============================================================
// Unary operators
// ============================================================

// ทดสอบ unary minus — ได้ (0 - x)
TEST(ParserUnaryTest, UnaryMinus) { EXPECT_EQ(parse_str("-x"), "0 - x"); }

// ทดสอบ unary minus กับตัวเลข
TEST(ParserUnaryTest, UnaryMinusNumber) { EXPECT_EQ(parse_str("-5"), "0 - 5"); }

// ทดสอบ unary plus (ไม่เปลี่ยนค่า)
TEST(ParserUnaryTest, UnaryPlus) { EXPECT_EQ(parse_str("+x"), "x"); }

// ทดสอบ double unary minus: --x = -(0-x) = (0 - (0 - x))
TEST(ParserUnaryTest, DoubleUnaryMinus) {
    EXPECT_EQ(parse_str("--x"), "0 - (0 - x)");
}

// ============================================================
// Implicit multiplication
// ============================================================

// ทดสอบ implicit multiplication: "2x" = "2 * x"
TEST(ParserImplicitMulTest, NumberVariable) {
    EXPECT_EQ(parse_str("2x"), "2*x");
}

// ทดสอบ implicit multiplication: "3(x)" = "3 * x"
TEST(ParserImplicitMulTest, NumberParen) {
    EXPECT_EQ(parse_str("3(x)"), "3*x");
}

// ทดสอบ implicit multiplication: "x y" = "x * y"
TEST(ParserImplicitMulTest, VariableVariable) {
    EXPECT_EQ(parse_str("x y"), "x*y");
}

// ทดสอบ implicit multiplication ซ้อนกัน: "2x y" = "(2*x)*y"
TEST(ParserImplicitMulTest, ChainedImplicit) {
    EXPECT_EQ(parse_str("2x y"), "2*x*y");
}

// ============================================================
// Complex expressions
// ============================================================

// ทดสอบ expression ซับซ้อน: "2x + 3y - 1"
TEST(ParserComplexTest, LinearExpression) {
    EXPECT_EQ(parse_str("2x + 3y - 1"), "2*x + 3*y - 1");
}

// ทดสอบ: "x^2 + 2x + 1"
TEST(ParserComplexTest, QuadraticLike) {
    EXPECT_EQ(parse_str("x^2 + 2x + 1"), "x^2 + 2*x + 1");
}

// ============================================================
// parse_equation()
// ============================================================

// ทดสอบ parse equation: "x = 5"
TEST(ParserEquationTest, SimpleEquation) {
    Parser parser("x = 5");
    auto   eq = parser.parse_equation();
    EXPECT_EQ(eq->to_string(), "x = 5");
}

// ทดสอบ parse equation: "2x + 1 = 7"
TEST(ParserEquationTest, LinearEquation) {
    Parser parser("2x + 1 = 7");
    auto   eq = parser.parse_equation();
    EXPECT_EQ(eq->to_string(), "2*x + 1 = 7");
}

// ทดสอบ parse equation fail — ไม่มี '='
TEST(ParserEquationTest, NoEquals) {
    Parser parser("2x + 1");
    EXPECT_THROW(parser.parse_equation(), ParseError);
}

// ทดสอบ parse equation fail — ไม่มี rhs
TEST(ParserEquationTest, NoRhs) {
    Parser parser("x =");
    EXPECT_THROW(parser.parse_equation(), ParseError);
}

// ============================================================
// parse_expression_or_equation()
// ============================================================

// ทดสอบ parse_expression_or_equation กับ expression
TEST(ParserExprOrEqTest, ExpressionPath) {
    Parser parser("1 + 2");
    auto [expr, eq] = parser.parse_expression_or_equation();
    EXPECT_NE(expr, nullptr);
    EXPECT_EQ(eq, nullptr);
    EXPECT_EQ(expr->to_string(), "1 + 2");
}

// ทดสอบ parse_expression_or_equation กับ equation
TEST(ParserExprOrEqTest, EquationPath) {
    Parser parser("x = 10");
    auto [expr, eq] = parser.parse_expression_or_equation();
    EXPECT_EQ(expr, nullptr);
    EXPECT_NE(eq, nullptr);
    EXPECT_EQ(eq->to_string(), "x = 10");
}

// ============================================================
// Error cases
// ============================================================

// ทดสอบ unmatched parenthesis
TEST(ParserErrorTest, UnmatchedLParen) {
    Parser parser("(1 + 2");
    EXPECT_THROW(parser.parse(), ParseError);
}

// ทดสอบ unexpected token ที่จุดเริ่มต้น
TEST(ParserErrorTest, UnexpectedTokenAtStart) {
    Parser parser(")");
    EXPECT_THROW(parser.parse(), ParseError);
}

// ทดสอบ trailing garbage
TEST(ParserErrorTest, TrailingGarbage) {
    Parser parser("1 + 2 )");
    EXPECT_THROW(parser.parse(), ParseError);
}

// ทดสอบ empty expression
TEST(ParserErrorTest, EmptyExpression) {
    Parser parser("");
    EXPECT_THROW(parser.parse(), ParseError);
}

// ทดสอบ trailing input after equation (เช่น ')' ที่ parser ไม่รู้จักในตำแหน่งนี้)
TEST(ParserErrorTest, TrailingAfterEquation) {
    Parser parser("x = 5 )");
    EXPECT_THROW(parser.parse_equation(), ParseError);
}

// ============================================================
// input() accessor
// ============================================================

TEST(ParserTest, InputAccessor) {
    Parser parser("hello");
    EXPECT_EQ(parser.input(), "hello");
}
