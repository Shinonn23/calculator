#include <gtest/gtest.h>

#include "ast/helpers.hpp"
#include "ast/math/binary_expr.hpp"
#include "ast/math/number_expr.hpp"
#include "ast/math/variable_expr.hpp"
#include "ast/mock_visitors.hpp"

using namespace math_solver;
using test_helpers::MockExprVisitor;
using namespace test_helpers;

TEST(ExprVisitorTest, NumberDispatchesToNumber) {
    MockExprVisitor v;
    Number(1.0).accept(v);
    EXPECT_EQ(v.last, MockExprVisitor::Visited::Number);
}

TEST(ExprVisitorTest, VariableDispatchesToVariable) {
    MockExprVisitor v;
    Variable("x").accept(v);
    EXPECT_EQ(v.last, MockExprVisitor::Visited::Variable);
}

TEST(ExprVisitorTest, BinaryOpDispatchesToBinaryOp) {
    MockExprVisitor v;
    BinaryOp(make_num(1), make_num(2), BinaryOpType::Add).accept(v);
    EXPECT_EQ(v.last, MockExprVisitor::Visited::BinaryOp);
}

TEST(ExprVisitorTest, DispatchUpdatesLastVisited) {
    MockExprVisitor v;
    Number(1.0).accept(v);
    EXPECT_EQ(v.last, MockExprVisitor::Visited::Number);
    Variable("x").accept(v);
    EXPECT_EQ(v.last, MockExprVisitor::Visited::Variable);
    BinaryOp(make_num(0), make_num(0), BinaryOpType::Add).accept(v);
    EXPECT_EQ(v.last, MockExprVisitor::Visited::BinaryOp);
}

TEST(ExprVisitorTest, VisitViaBasePointer) {
    MockExprVisitor v;
    std::unique_ptr<Expr> expr = std::make_unique<Number>(42.0);
    expr->accept(v);
    EXPECT_EQ(v.last, MockExprVisitor::Visited::Number);
}
