#include <gtest/gtest.h>

#include "ast/math/binary_expr.hpp"
#include "ast/helpers.hpp"
#include "ast/mock_visitors.hpp"

using namespace math_solver;
using namespace test_helpers;

// ── Accessors ──────────────────────────────────────────────────────────────

TEST(BinaryOpTest, OpStored) {
    BinaryOp b(make_num(1), make_num(2), BinaryOpType::Add);
    EXPECT_EQ(b.op(), BinaryOpType::Add);
}

TEST(BinaryOpTest, LeftAndRightStored) {
    BinaryOp b(make_num(3), make_var("x"), BinaryOpType::Mul);
    EXPECT_EQ(b.left().to_string(), "3");
    EXPECT_EQ(b.right().to_string(), "x");
}

// ── Span ──────────────────────────────────────────────────────────────────

TEST(BinaryOpTest, SpanAutoMergesChildren) {
    BinaryOp b(make_num(1, 0, 1), make_num(2, 4, 5), BinaryOpType::Add);
    EXPECT_EQ(b.span().start, 0u);
    EXPECT_EQ(b.span().end, 5u);
}

TEST(BinaryOpTest, ExplicitSpanOverridesChildMerge) {
    BinaryOp b(make_num(1, 0, 1), make_num(2, 4, 5), BinaryOpType::Sub, Span(0, 10));
    EXPECT_EQ(b.span().start, 0u);
    EXPECT_EQ(b.span().end, 10u);
}

// ── to_string ────────────────────────────────────────────────────────────

TEST(BinaryOpTest, ToStringAdd) {
    EXPECT_EQ(BinaryOp(make_num(1), make_num(2), BinaryOpType::Add).to_string(), "(1 + 2)");
}

TEST(BinaryOpTest, ToStringSub) {
    EXPECT_EQ(BinaryOp(make_num(5), make_num(3), BinaryOpType::Sub).to_string(), "(5 - 3)");
}

TEST(BinaryOpTest, ToStringMul) {
    EXPECT_EQ(BinaryOp(make_var("a"), make_var("b"), BinaryOpType::Mul).to_string(), "(a * b)");
}

TEST(BinaryOpTest, ToStringDiv) {
    EXPECT_EQ(BinaryOp(make_num(6), make_num(2), BinaryOpType::Div).to_string(), "(6 / 2)");
}

TEST(BinaryOpTest, ToStringPow) {
    EXPECT_EQ(BinaryOp(make_var("x"), make_num(2), BinaryOpType::Pow).to_string(), "(x ^ 2)");
}

TEST(BinaryOpTest, ToStringNested) {
    auto inner = std::make_unique<BinaryOp>(make_num(1), make_num(2), BinaryOpType::Add);
    BinaryOp outer(std::move(inner), make_num(3), BinaryOpType::Mul);
    EXPECT_EQ(outer.to_string(), "((1 + 2) * 3)");
}

TEST(BinaryOpTest, ToStringNeverContainsQuestionMark) {
    for (auto op : {BinaryOpType::Add, BinaryOpType::Sub, BinaryOpType::Mul,
                    BinaryOpType::Div, BinaryOpType::Pow}) {
        EXPECT_EQ(BinaryOp(make_num(1), make_num(2), op).to_string().find('?'),
                  std::string::npos)
            << "Unexpected '?' for op " << static_cast<int>(op);
    }
}

// ── clone ────────────────────────────────────────────────────────────────

TEST(BinaryOpTest, ClonePreservesOpAndOperands) {
    BinaryOp b(make_num(4), make_var("y"), BinaryOpType::Div, Span(0, 5));
    auto     c = b.clone();
    auto&    cb = static_cast<BinaryOp&>(*c);
    EXPECT_EQ(cb.op(), BinaryOpType::Div);
    EXPECT_EQ(cb.left().to_string(), "4");
    EXPECT_EQ(cb.right().to_string(), "y");
}

TEST(BinaryOpTest, ClonePreservesSpan) {
    BinaryOp b(make_num(1, 0, 1), make_num(2, 2, 3), BinaryOpType::Add);
    auto     c = b.clone();
    EXPECT_EQ(c->span().start, b.span().start);
    EXPECT_EQ(c->span().end, b.span().end);
}

TEST(BinaryOpTest, CloneIsIndependent) {
    BinaryOp b(make_num(1), make_num(2), BinaryOpType::Add);
    EXPECT_NE(b.clone().get(), static_cast<Expr*>(&b));
}

// ── visitor dispatch ────────────────────────────────────────────────────

TEST(BinaryOpTest, AcceptDispatchesToVisitor) {
    BinaryOp        b(make_num(1), make_num(2), BinaryOpType::Add);
    MockExprVisitor v;
    b.accept(v);
    EXPECT_EQ(v.last, MockExprVisitor::Visited::BinaryOp);
}
