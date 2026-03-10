#include <gtest/gtest.h>

#include "ast/math/array_expr.hpp"
#include "ast/math/binary_expr.hpp"
#include "ast/math/call_expr.hpp"
#include "ast/math/equation_expr.hpp"
#include "ast/math/number_expr.hpp"
#include "ast/math/unary_expr.hpp"
#include "ast/math/variable_expr.hpp"
#include "parser/math/math_parser.hpp"

using namespace math_solver;

// ============================================================
// Helpers
// ============================================================

static ExprPtr parse_ok(const std::string& input) {
    Parser p(input);
    auto   r = p.parse();
    EXPECT_TRUE(r.ok()) << "parse error for: " << input;
    return r.ok() ? std::move(*r) : nullptr;
}

static void parse_fail(const std::string& input) {
    Parser p(input);
    EXPECT_FALSE(p.parse().ok()) << "expected failure for: " << input;
}

template <typename T>
static const T& as(const Expr& e, const char* label = "") {
    const T* p = dynamic_cast<const T*>(&e);
    EXPECT_NE(p, nullptr) << "wrong node type for: " << label;
    static T dummy{};
    return p ? *p : dummy;
}

// ============================================================
// Number literals
// ============================================================

TEST(MathParser, Integer) {
    auto e = parse_ok("42");
    ASSERT_NE(e, nullptr);
    EXPECT_DOUBLE_EQ(dynamic_cast<const Number&>(*e).value(), 42.0);
}

TEST(MathParser, Zero) {
    auto e = parse_ok("0");
    ASSERT_NE(e, nullptr);
    EXPECT_DOUBLE_EQ(dynamic_cast<const Number&>(*e).value(), 0.0);
}

TEST(MathParser, Float) {
    auto e = parse_ok("3.14");
    ASSERT_NE(e, nullptr);
    EXPECT_DOUBLE_EQ(dynamic_cast<const Number&>(*e).value(), 3.14);
}

TEST(MathParser, LeadingDotFloat) {
    auto e = parse_ok(".5");
    ASSERT_NE(e, nullptr);
    EXPECT_DOUBLE_EQ(dynamic_cast<const Number&>(*e).value(), 0.5);
}

// ============================================================
// Variables
// ============================================================

TEST(MathParser, SingleLetterVar) {
    auto e = parse_ok("x");
    ASSERT_NE(e, nullptr);
    EXPECT_EQ(dynamic_cast<const Variable&>(*e).name(), "x");
}

TEST(MathParser, MultiLetterVar) {
    auto e = parse_ok("abc");
    ASSERT_NE(e, nullptr);
    EXPECT_EQ(dynamic_cast<const Variable&>(*e).name(), "abc");
}

TEST(MathParser, UnderscoreVar) {
    auto e = parse_ok("my_var");
    ASSERT_NE(e, nullptr);
    EXPECT_EQ(dynamic_cast<const Variable&>(*e).name(), "my_var");
}

// ============================================================
// Binary operators — basic
// ============================================================

TEST(MathParser, Addition) {
    auto e = parse_ok("1 + 2");
    ASSERT_NE(e, nullptr);
    auto& b = dynamic_cast<const BinaryOp&>(*e);
    EXPECT_EQ(b.op(), BinaryOpType::Add);
    EXPECT_DOUBLE_EQ(dynamic_cast<const Number&>(b.left()).value(), 1.0);
    EXPECT_DOUBLE_EQ(dynamic_cast<const Number&>(b.right()).value(), 2.0);
}

TEST(MathParser, Subtraction) {
    auto e = parse_ok("5 - 3");
    ASSERT_NE(e, nullptr);
    EXPECT_EQ(dynamic_cast<const BinaryOp&>(*e).op(), BinaryOpType::Sub);
}

TEST(MathParser, Multiplication) {
    auto e = parse_ok("4 * 7");
    ASSERT_NE(e, nullptr);
    EXPECT_EQ(dynamic_cast<const BinaryOp&>(*e).op(), BinaryOpType::Mul);
}

TEST(MathParser, Division) {
    auto e = parse_ok("10 / 2");
    ASSERT_NE(e, nullptr);
    EXPECT_EQ(dynamic_cast<const BinaryOp&>(*e).op(), BinaryOpType::Div);
}

TEST(MathParser, Power) {
    auto e = parse_ok("2 ^ 3");
    ASSERT_NE(e, nullptr);
    EXPECT_EQ(dynamic_cast<const BinaryOp&>(*e).op(), BinaryOpType::Pow);
}

// ============================================================
// Precedence & associativity
// ============================================================

TEST(MathParser, MulBindsTighterThanAdd) {
    // 1 + 2 * 3  →  Add(1, Mul(2, 3))
    auto e = parse_ok("1 + 2 * 3");
    ASSERT_NE(e, nullptr);
    auto& b = dynamic_cast<const BinaryOp&>(*e);
    EXPECT_EQ(b.op(), BinaryOpType::Add);
    EXPECT_EQ(dynamic_cast<const BinaryOp&>(b.right()).op(), BinaryOpType::Mul);
}

TEST(MathParser, PowBindsTighterThanMul) {
    // 2 * 3 ^ 2  →  Mul(2, Pow(3, 2))
    auto e = parse_ok("2 * 3 ^ 2");
    ASSERT_NE(e, nullptr);
    auto& b = dynamic_cast<const BinaryOp&>(*e);
    EXPECT_EQ(b.op(), BinaryOpType::Mul);
    EXPECT_EQ(dynamic_cast<const BinaryOp&>(b.right()).op(), BinaryOpType::Pow);
}

TEST(MathParser, PowIsRightAssociative) {
    // 2 ^ 3 ^ 2  →  Pow(2, Pow(3, 2))
    auto e = parse_ok("2 ^ 3 ^ 2");
    ASSERT_NE(e, nullptr);
    auto& outer = dynamic_cast<const BinaryOp&>(*e);
    EXPECT_EQ(outer.op(), BinaryOpType::Pow);
    EXPECT_DOUBLE_EQ(dynamic_cast<const Number&>(outer.left()).value(), 2.0);
    auto& inner = dynamic_cast<const BinaryOp&>(outer.right());
    EXPECT_EQ(inner.op(), BinaryOpType::Pow);
    EXPECT_DOUBLE_EQ(dynamic_cast<const Number&>(inner.left()).value(), 3.0);
}

TEST(MathParser, AddIsLeftAssociative) {
    // 1 + 2 + 3  →  Add(Add(1, 2), 3)
    auto e = parse_ok("1 + 2 + 3");
    ASSERT_NE(e, nullptr);
    auto& outer = dynamic_cast<const BinaryOp&>(*e);
    EXPECT_EQ(outer.op(), BinaryOpType::Add);
    EXPECT_DOUBLE_EQ(dynamic_cast<const Number&>(outer.right()).value(), 3.0);
    EXPECT_EQ(dynamic_cast<const BinaryOp&>(outer.left()).op(),
              BinaryOpType::Add);
}

TEST(MathParser, MulIsLeftAssociative) {
    // 6 / 2 / 3  →  Div(Div(6, 2), 3)
    auto e = parse_ok("6 / 2 / 3");
    ASSERT_NE(e, nullptr);
    auto& outer = dynamic_cast<const BinaryOp&>(*e);
    EXPECT_EQ(outer.op(), BinaryOpType::Div);
    EXPECT_EQ(dynamic_cast<const BinaryOp&>(outer.left()).op(),
              BinaryOpType::Div);
}

// ============================================================
// Parentheses
// ============================================================

TEST(MathParser, ParensOverridePrecedence) {
    // (1 + 2) * 3  →  Mul(Add(1, 2), 3)
    auto e = parse_ok("(1 + 2) * 3");
    ASSERT_NE(e, nullptr);
    auto& b = dynamic_cast<const BinaryOp&>(*e);
    EXPECT_EQ(b.op(), BinaryOpType::Mul);
    EXPECT_EQ(dynamic_cast<const BinaryOp&>(b.left()).op(), BinaryOpType::Add);
}

TEST(MathParser, NestedParens) {
    auto e = parse_ok("((x))");
    ASSERT_NE(e, nullptr);
    EXPECT_NE(dynamic_cast<const Variable*>(e.get()), nullptr);
}

TEST(MathParser, ParensAroundNumber) {
    auto e = parse_ok("(42)");
    ASSERT_NE(e, nullptr);
    EXPECT_DOUBLE_EQ(dynamic_cast<const Number&>(*e).value(), 42.0);
}

// ============================================================
// Unary minus
// ============================================================

TEST(MathParser, UnaryMinusNumber) {
    auto e = parse_ok("-5");
    ASSERT_NE(e, nullptr);
    auto& u = dynamic_cast<const UnaryOp&>(*e);
    EXPECT_EQ(u.op(), UnaryOpType::Neg);
    EXPECT_DOUBLE_EQ(dynamic_cast<const Number&>(u.operand()).value(), 5.0);
}

TEST(MathParser, UnaryMinusVar) {
    auto e = parse_ok("-x");
    ASSERT_NE(e, nullptr);
    auto& u = dynamic_cast<const UnaryOp&>(*e);
    EXPECT_EQ(u.op(), UnaryOpType::Neg);
    EXPECT_EQ(dynamic_cast<const Variable&>(u.operand()).name(), "x");
}

TEST(MathParser, DoubleUnaryMinus) {
    auto e = parse_ok("--x");
    ASSERT_NE(e, nullptr);
    auto& outer = dynamic_cast<const UnaryOp&>(*e);
    auto& inner = dynamic_cast<const UnaryOp&>(outer.operand());
    EXPECT_EQ(dynamic_cast<const Variable&>(inner.operand()).name(), "x");
}

TEST(MathParser, UnaryMinusInExpr) {
    // -x + 1  →  Add(Neg(x), 1)
    auto e = parse_ok("-x + 1");
    ASSERT_NE(e, nullptr);
    auto& b = dynamic_cast<const BinaryOp&>(*e);
    EXPECT_EQ(b.op(), BinaryOpType::Add);
    EXPECT_NE(dynamic_cast<const UnaryOp*>(&b.left()), nullptr);
}

// ============================================================
// Implicit multiplication
// ============================================================

TEST(MathParser, ImplicitMulNumberVar) {
    // 2x  →  Mul(2, x)
    auto e = parse_ok("2x");
    ASSERT_NE(e, nullptr);
    auto& b = dynamic_cast<const BinaryOp&>(*e);
    EXPECT_EQ(b.op(), BinaryOpType::Mul);
    EXPECT_DOUBLE_EQ(dynamic_cast<const Number&>(b.left()).value(), 2.0);
    EXPECT_EQ(dynamic_cast<const Variable&>(b.right()).name(), "x");
}

TEST(MathParser, ImplicitMulNumberParen) {
    // 3(x+1)  →  Mul(3, Add(x, 1))
    auto e = parse_ok("3(x+1)");
    ASSERT_NE(e, nullptr);
    auto& b = dynamic_cast<const BinaryOp&>(*e);
    EXPECT_EQ(b.op(), BinaryOpType::Mul);
    EXPECT_DOUBLE_EQ(dynamic_cast<const Number&>(b.left()).value(), 3.0);
    EXPECT_EQ(dynamic_cast<const BinaryOp&>(b.right()).op(), BinaryOpType::Add);
}

// ============================================================
// Built-in function calls
// ============================================================

TEST(MathParser, FunctionCallSin) {
    auto e = parse_ok("sin(x)");
    ASSERT_NE(e, nullptr);
    auto& f = dynamic_cast<const FunctionCall&>(*e);
    EXPECT_EQ(f.name(), "sin");
    EXPECT_EQ(f.kind(), FuncKind::Sin);
    EXPECT_EQ(dynamic_cast<const Variable&>(f.arg()).name(), "x");
}

TEST(MathParser, FunctionCallCos) {
    auto e = parse_ok("cos(0)");
    ASSERT_NE(e, nullptr);
    auto& f = dynamic_cast<const FunctionCall&>(*e);
    EXPECT_EQ(f.name(), "cos");
    EXPECT_EQ(f.kind(), FuncKind::Cos);
}

TEST(MathParser, FunctionCallSqrt) {
    auto e = parse_ok("sqrt(2)");
    ASSERT_NE(e, nullptr);
    auto& f = dynamic_cast<const FunctionCall&>(*e);
    EXPECT_EQ(f.kind(), FuncKind::Sqrt);
}

TEST(MathParser, FunctionCallAbs) {
    auto e = parse_ok("abs(x)");
    ASSERT_NE(e, nullptr);
    EXPECT_EQ(dynamic_cast<const FunctionCall&>(*e).kind(), FuncKind::Abs);
}

TEST(MathParser, FunctionCallWithExprArg) {
    auto e = parse_ok("abs(x + 1)");
    ASSERT_NE(e, nullptr);
    auto& f = dynamic_cast<const FunctionCall&>(*e);
    EXPECT_EQ(dynamic_cast<const BinaryOp&>(f.arg()).op(), BinaryOpType::Add);
}

TEST(MathParser, FunctionCallNestedInExpr) {
    // sin(x) + 1  →  Add(FuncCall(sin, x), 1)
    auto e = parse_ok("sin(x) + 1");
    ASSERT_NE(e, nullptr);
    auto& b = dynamic_cast<const BinaryOp&>(*e);
    EXPECT_EQ(b.op(), BinaryOpType::Add);
    EXPECT_NE(dynamic_cast<const FunctionCall*>(&b.left()), nullptr);
}

// ============================================================
// Array literals
// ============================================================

TEST(MathParser, EmptyArray) {
    auto e = parse_ok("[]");
    ASSERT_NE(e, nullptr);
    EXPECT_EQ(dynamic_cast<const ArrayExpr&>(*e).elements().size(), 0u);
}

TEST(MathParser, SingleElementArray) {
    auto e = parse_ok("[1]");
    ASSERT_NE(e, nullptr);
    auto& a = dynamic_cast<const ArrayExpr&>(*e);
    ASSERT_EQ(a.elements().size(), 1u);
    EXPECT_DOUBLE_EQ(dynamic_cast<const Number&>(*a.elements()[0]).value(),
                     1.0);
}

TEST(MathParser, ThreeElementArray) {
    auto e = parse_ok("[1, 2, 3]");
    ASSERT_NE(e, nullptr);
    auto& a = dynamic_cast<const ArrayExpr&>(*e);
    ASSERT_EQ(a.elements().size(), 3u);
    EXPECT_DOUBLE_EQ(dynamic_cast<const Number&>(*a.elements()[0]).value(),
                     1.0);
    EXPECT_DOUBLE_EQ(dynamic_cast<const Number&>(*a.elements()[1]).value(),
                     2.0);
    EXPECT_DOUBLE_EQ(dynamic_cast<const Number&>(*a.elements()[2]).value(),
                     3.0);
}

TEST(MathParser, ArrayWithExprElements) {
    auto e = parse_ok("[x+1, 2*y]");
    ASSERT_NE(e, nullptr);
    auto& a = dynamic_cast<const ArrayExpr&>(*e);
    ASSERT_EQ(a.elements().size(), 2u);
    EXPECT_EQ(dynamic_cast<const BinaryOp&>(*a.elements()[0]).op(),
              BinaryOpType::Add);
}

// ============================================================
// Equation parsing
// ============================================================

TEST(MathParser, SimpleEquationZeroRhs) {
    Parser p("x + 1 = 0");
    auto   r = p.parse_equation();
    ASSERT_TRUE(r.ok());
    EXPECT_EQ(dynamic_cast<const BinaryOp&>((*r)->lhs()).op(),
              BinaryOpType::Add);
    EXPECT_DOUBLE_EQ(dynamic_cast<const Number&>((*r)->rhs()).value(), 0.0);
}

TEST(MathParser, EquationBothSidesNonTrivial) {
    Parser p("2*x + 1 = x - 3");
    auto   r = p.parse_equation();
    ASSERT_TRUE(r.ok());
    EXPECT_NE(dynamic_cast<const BinaryOp*>(&(*r)->lhs()), nullptr);
    EXPECT_NE(dynamic_cast<const BinaryOp*>(&(*r)->rhs()), nullptr);
}

TEST(MathParser, LinearEquationSingleVar) {
    Parser p("3*x = 9");
    auto   r = p.parse_equation();
    ASSERT_TRUE(r.ok());
}

// ============================================================
// parse_expression_or_equation
// ============================================================

TEST(MathParser, ExpressionOrEquation_Expr) {
    Parser p("x + 1");
    auto   r = p.parse_expression_or_equation();
    ASSERT_TRUE(r.ok());
    EXPECT_NE((*r).first, nullptr);  // expression is populated
    EXPECT_EQ((*r).second, nullptr); // equation is null
}

TEST(MathParser, ExpressionOrEquation_Equation) {
    Parser p("x + 1 = 0");
    auto   r = p.parse_expression_or_equation();
    ASSERT_TRUE(r.ok());
    EXPECT_EQ((*r).first, nullptr);  // expression is null
    EXPECT_NE((*r).second, nullptr); // equation is populated
}

// ============================================================
// to_string structural checks
// ============================================================

TEST(MathParser, ToStringAdd) {
    auto e = parse_ok("1 + 2");
    ASSERT_NE(e, nullptr);
    EXPECT_EQ(e->to_string(), "(1 + 2)");
}

TEST(MathParser, ToStringNestedPrecedence) {
    auto e = parse_ok("1 + 2 * 3");
    ASSERT_NE(e, nullptr);
    EXPECT_EQ(e->to_string(), "(1 + (2 * 3))");
}

TEST(MathParser, ToStringNeg) {
    auto e = parse_ok("-x");
    ASSERT_NE(e, nullptr);
    EXPECT_EQ(e->to_string(), "(-x)");
}

TEST(MathParser, ToStringFunctionCall) {
    auto e = parse_ok("sin(x)");
    ASSERT_NE(e, nullptr);
    EXPECT_EQ(e->to_string(), "sin(x)");
}

// ============================================================
// Error cases
// ============================================================

TEST(MathParser, EmptyIsError) { parse_fail(""); }
TEST(MathParser, UnmatchedLParenIsError) { parse_fail("(1 + 2"); }
TEST(MathParser, TrailingOperatorIsError) { parse_fail("1 +"); }
TEST(MathParser, ReservedKeywordIsError) { parse_fail("solve + 1"); }
