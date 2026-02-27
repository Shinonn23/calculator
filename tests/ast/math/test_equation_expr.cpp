#include <gtest/gtest.h>

#include "ast/math/equation_expr.hpp"
#include "ast/helpers.hpp"

using namespace math_solver;
using namespace test_helpers;

TEST(EquationTest, LhsAndRhsStored) {
    Equation eq(make_num(1), make_var("x"));
    EXPECT_EQ(eq.lhs().to_string(), "1");
    EXPECT_EQ(eq.rhs().to_string(), "x");
}

TEST(EquationTest, SpanAutoMergesChildren) {
    Equation eq(make_num(0, 0, 1), make_var("x", 4, 5));
    EXPECT_EQ(eq.span().start, 0u);
    EXPECT_EQ(eq.span().end, 5u);
}

TEST(EquationTest, ExplicitSpanStored) {
    Equation eq(make_num(0, 0, 1), make_var("x", 4, 5), Span(0, 10));
    EXPECT_EQ(eq.span().start, 0u);
    EXPECT_EQ(eq.span().end, 10u);
}

TEST(EquationTest, ToString) {
    EXPECT_EQ(Equation(make_num(3), make_var("x")).to_string(), "3 = x");
}

TEST(EquationTest, ToStringBothSidesReflected) {
    EXPECT_EQ(Equation(make_var("a"), make_var("b")).to_string(), "a = b");
}

TEST(EquationTest, CloneIsDeepCopy) {
    Equation eq(make_num(2), make_var("y"), Span(0, 5));
    auto     c = eq.clone();
    ASSERT_NE(c.get(), &eq);
    EXPECT_EQ(c->to_string(), "2 = y");
    EXPECT_EQ(c->span().start, 0u);
    EXPECT_EQ(c->span().end, 5u);
}

TEST(EquationTest, TakeLhsTransfersOwnership) {
    Equation eq(make_num(7), make_var("z"));
    auto     lhs = eq.take_lhs();
    ASSERT_NE(lhs, nullptr);
    EXPECT_EQ(lhs->to_string(), "7");
}

TEST(EquationTest, TakeRhsTransfersOwnership) {
    Equation eq(make_num(1), make_var("w"));
    auto     rhs = eq.take_rhs();
    ASSERT_NE(rhs, nullptr);
    EXPECT_EQ(rhs->to_string(), "w");
}
