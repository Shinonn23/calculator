#include <gtest/gtest.h>

#include "ast/math/number_expr.hpp"
#include "ast/mock_visitors.hpp"

using namespace math_solver;
using test_helpers::MockExprVisitor;

TEST(NumberTest, ValueStored) {
    EXPECT_DOUBLE_EQ(Number(3.14).value(), 3.14);
}

TEST(NumberTest, NegativeValue) {
    EXPECT_DOUBLE_EQ(Number(-7.5).value(), -7.5);
}

TEST(NumberTest, ZeroValue) {
    EXPECT_DOUBLE_EQ(Number(0.0).value(), 0.0);
}

TEST(NumberTest, DefaultSpanIsEmpty) {
    EXPECT_TRUE(Number(1.0).span().empty());
}

TEST(NumberTest, ExplicitSpanStored) {
    Number n(1.0, Span(2, 5));
    EXPECT_EQ(n.span().start, 2u);
    EXPECT_EQ(n.span().end, 5u);
}

TEST(NumberTest, ToStringInteger) {
    EXPECT_EQ(Number(5.0).to_string(), "5");
    EXPECT_EQ(Number(0.0).to_string(), "0");
    EXPECT_EQ(Number(-2.0).to_string(), "-2");
}

TEST(NumberTest, ToStringTrimsTrailingZeros) {
    std::string s = Number(1.5).to_string();
    EXPECT_NE(s.find("1.5"), std::string::npos);
    EXPECT_NE(s.back(), '0');
}

TEST(NumberTest, ToStringDoesNotEndWithDot) {
    EXPECT_EQ(Number(3.0).to_string().back() != '.', true);
}

TEST(NumberTest, ClonePreservesValue) {
    Number n(42.0, Span(1, 3));
    auto   c = n.clone();
    EXPECT_DOUBLE_EQ(static_cast<Number&>(*c).value(), 42.0);
}

TEST(NumberTest, ClonePreservesSpan) {
    Number n(7.0, Span(4, 6));
    auto   c = n.clone();
    EXPECT_EQ(c->span().start, 4u);
    EXPECT_EQ(c->span().end, 6u);
}

TEST(NumberTest, CloneIsIndependent) {
    Number n(1.0);
    EXPECT_NE(n.clone().get(), static_cast<Expr*>(&n));
}

TEST(NumberTest, AcceptDispatchesToVisitor) {
    Number          n(1.0);
    MockExprVisitor v;
    n.accept(v);
    EXPECT_EQ(v.last, MockExprVisitor::Visited::Number);
}
