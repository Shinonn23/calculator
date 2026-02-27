#include <gtest/gtest.h>

#include "ast/math/variable_expr.hpp"
#include "ast/mock_visitors.hpp"

using namespace math_solver;
using test_helpers::MockExprVisitor;

TEST(VariableTest, NameStored) {
    EXPECT_EQ(Variable("x").name(), "x");
    EXPECT_EQ(Variable("alpha").name(), "alpha");
}

TEST(VariableTest, DefaultSpanIsEmpty) {
    EXPECT_TRUE(Variable("y").span().empty());
}

TEST(VariableTest, ExplicitSpanStored) {
    Variable v("z", Span(1, 2));
    EXPECT_EQ(v.span().start, 1u);
    EXPECT_EQ(v.span().end, 2u);
}

TEST(VariableTest, ToStringEqualsName) {
    EXPECT_EQ(Variable("beta").to_string(), "beta");
    EXPECT_EQ(Variable("x1").to_string(), "x1");
}

TEST(VariableTest, ClonePreservesNameAndSpan) {
    Variable v("beta", Span(3, 7));
    auto     c = v.clone();
    auto&    cv = static_cast<Variable&>(*c);
    EXPECT_EQ(cv.name(), "beta");
    EXPECT_EQ(cv.span().start, 3u);
    EXPECT_EQ(cv.span().end, 7u);
}

TEST(VariableTest, CloneIsIndependent) {
    Variable v("x");
    EXPECT_NE(v.clone().get(), static_cast<Expr*>(&v));
}

TEST(VariableTest, AcceptDispatchesToVisitor) {
    Variable        v("x");
    MockExprVisitor vis;
    v.accept(vis);
    EXPECT_EQ(vis.last, MockExprVisitor::Visited::Variable);
}
