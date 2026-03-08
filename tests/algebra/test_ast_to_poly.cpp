#include <gtest/gtest.h>

#include "algebra/polynomial/ast_to_poly.hpp"
#include "ast/math/array_expr.hpp"
#include "ast/math/binary_expr.hpp"
#include "ast/math/call_expr.hpp"
#include "ast/math/number_expr.hpp"
#include "ast/math/unary_expr.hpp"
#include "ast/math/variable_expr.hpp"

using namespace math_solver;

// ─── helpers ────────────────────────────────────────────────────────────────

static ExprPtr num(double v) { return std::make_unique<Number>(v); }
static ExprPtr var(const std::string& n) { return std::make_unique<Variable>(n); }
static ExprPtr binop(ExprPtr l, ExprPtr r, BinaryOpType op) {
    return std::make_unique<BinaryOp>(std::move(l), std::move(r), op);
}
static ExprPtr unary_neg(ExprPtr e) {
    return std::make_unique<UnaryOp>(std::move(e), UnaryOpType::Neg);
}

static Polynomial convert_ok(const Expr& e) {
    ASTToPolynomial conv;
    auto r = conv.convert(e);
    EXPECT_TRUE(r.ok()) << "unexpected error";
    return r.ok() ? *r : Polynomial();
}

static void convert_fail(const Expr& e) {
    ASTToPolynomial conv;
    auto r = conv.convert(e);
    EXPECT_FALSE(r.ok()) << "expected conversion to fail";
}

// ─── number ─────────────────────────────────────────────────────────────────

TEST(ASTToPoly, Number) {
    auto e = num(5.0);
    Polynomial p = convert_ok(*e);
    EXPECT_TRUE(p.is_constant());
    EXPECT_DOUBLE_EQ(p.constant_value(), 5.0);
}

TEST(ASTToPoly, NegativeNumber) {
    auto e = num(-3.0);
    Polynomial p = convert_ok(*e);
    EXPECT_DOUBLE_EQ(p.constant_value(), -3.0);
}

TEST(ASTToPoly, Zero) {
    auto e = num(0.0);
    Polynomial p = convert_ok(*e);
    EXPECT_TRUE(p.is_zero());
}

// ─── variable ───────────────────────────────────────────────────────────────

TEST(ASTToPoly, Variable) {
    auto e = var("x");
    Polynomial p = convert_ok(*e);
    EXPECT_FALSE(p.is_constant());
    EXPECT_EQ(p.degree(), 1);
    EXPECT_DOUBLE_EQ(p.coefficient(Monomial("x", 1)), 1.0);
}

// ─── unary negation ──────────────────────────────────────────────────────────

TEST(ASTToPoly, UnaryNegNumber) {
    auto e = unary_neg(num(4.0));
    Polynomial p = convert_ok(*e);
    EXPECT_DOUBLE_EQ(p.constant_value(), -4.0);
}

TEST(ASTToPoly, UnaryNegVariable) {
    auto e = unary_neg(var("x"));
    Polynomial p = convert_ok(*e);
    EXPECT_DOUBLE_EQ(p.coefficient(Monomial("x", 1)), -1.0);
}

// ─── addition / subtraction ──────────────────────────────────────────────────

TEST(ASTToPoly, Add) {
    // x + 2
    auto e = binop(var("x"), num(2.0), BinaryOpType::Add);
    Polynomial p = convert_ok(*e);
    EXPECT_DOUBLE_EQ(p.coefficient(Monomial("x", 1)), 1.0);
    EXPECT_DOUBLE_EQ(p.constant_value(), 2.0);
}

TEST(ASTToPoly, Sub) {
    // x - 3
    auto e = binop(var("x"), num(3.0), BinaryOpType::Sub);
    Polynomial p = convert_ok(*e);
    EXPECT_DOUBLE_EQ(p.coefficient(Monomial("x", 1)), 1.0);
    EXPECT_DOUBLE_EQ(p.constant_value(), -3.0);
}

// ─── multiplication ──────────────────────────────────────────────────────────

TEST(ASTToPoly, MulVarByConst) {
    // 3 * x
    auto e = binop(num(3.0), var("x"), BinaryOpType::Mul);
    Polynomial p = convert_ok(*e);
    EXPECT_DOUBLE_EQ(p.coefficient(Monomial("x", 1)), 3.0);
}

TEST(ASTToPoly, MulVarByVar) {
    // x * x → x^2 (polynomial — this is fine, polynomials allow it)
    auto e = binop(var("x"), var("x"), BinaryOpType::Mul);
    Polynomial p = convert_ok(*e);
    EXPECT_DOUBLE_EQ(p.coefficient(Monomial("x", 2)), 1.0);
}

TEST(ASTToPoly, MulBinomial) {
    // (x + 1)(x - 1) = x^2 - 1
    auto lhs = binop(var("x"), num(1.0), BinaryOpType::Add);
    auto rhs = binop(var("x"), num(1.0), BinaryOpType::Sub);
    auto e = binop(std::move(lhs), std::move(rhs), BinaryOpType::Mul);
    Polynomial p = convert_ok(*e);
    EXPECT_DOUBLE_EQ(p.coeff_of_degree(2), 1.0);
    EXPECT_DOUBLE_EQ(p.coeff_of_degree(0), -1.0);
}

// ─── division ───────────────────────────────────────────────────────────────

TEST(ASTToPoly, DivByConstant) {
    // x / 2
    auto e = binop(var("x"), num(2.0), BinaryOpType::Div);
    Polynomial p = convert_ok(*e);
    EXPECT_DOUBLE_EQ(p.coefficient(Monomial("x", 1)), 0.5);
}

TEST(ASTToPoly, DivByVariable_Fails) {
    // x / x — not polynomial
    auto e = binop(var("x"), var("x"), BinaryOpType::Div);
    convert_fail(*e);
}

TEST(ASTToPoly, DivByZero_Fails) {
    // x / 0
    auto e = binop(var("x"), num(0.0), BinaryOpType::Div);
    convert_fail(*e);
}

// ─── exponentiation ─────────────────────────────────────────────────────────

TEST(ASTToPoly, PowByZero) {
    // x^0 = 1
    auto e = binop(var("x"), num(0.0), BinaryOpType::Pow);
    Polynomial p = convert_ok(*e);
    EXPECT_DOUBLE_EQ(p.constant_value(), 1.0);
}

TEST(ASTToPoly, PowByTwo) {
    // x^2
    auto e = binop(var("x"), num(2.0), BinaryOpType::Pow);
    Polynomial p = convert_ok(*e);
    EXPECT_DOUBLE_EQ(p.coefficient(Monomial("x", 2)), 1.0);
}

TEST(ASTToPoly, PowByThree) {
    // x^3
    auto e = binop(var("x"), num(3.0), BinaryOpType::Pow);
    Polynomial p = convert_ok(*e);
    EXPECT_DOUBLE_EQ(p.coefficient(Monomial("x", 3)), 1.0);
}

TEST(ASTToPoly, PowByNegative_Fails) {
    // x^(-1) — not a polynomial
    auto e = binop(var("x"), num(-1.0), BinaryOpType::Pow);
    convert_fail(*e);
}

TEST(ASTToPoly, PowByFraction_Fails) {
    // x^0.5 — not a polynomial
    auto e = binop(var("x"), num(0.5), BinaryOpType::Pow);
    convert_fail(*e);
}

TEST(ASTToPoly, PowByVariable_Fails) {
    // x^y — not a polynomial
    auto e = binop(var("x"), var("y"), BinaryOpType::Pow);
    convert_fail(*e);
}

// ─── function call / array ───────────────────────────────────────────────────

TEST(ASTToPoly, FunctionCall_Fails) {
    auto arg = var("x");
    auto e = std::make_unique<FunctionCall>("sin", FuncKind::Sin, std::move(arg));
    convert_fail(*e);
}

TEST(ASTToPoly, ArrayExpr_Fails) {
    std::vector<ExprPtr> elems;
    elems.push_back(num(1.0));
    auto e = std::make_unique<ArrayExpr>(std::move(elems));
    convert_fail(*e);
}

// ─── reuse after multiple calls ───────────────────────────────────────────────

TEST(ASTToPoly, ReuseConverter) {
    ASTToPolynomial conv;

    auto e1 = num(7.0);
    auto r1 = conv.convert(*e1);
    EXPECT_TRUE(r1.ok());
    EXPECT_DOUBLE_EQ((*r1).constant_value(), 7.0);

    auto e2 = var("y");
    auto r2 = conv.convert(*e2);
    EXPECT_TRUE(r2.ok());
    EXPECT_DOUBLE_EQ((*r2).coefficient(Monomial("y", 1)), 1.0);
}
