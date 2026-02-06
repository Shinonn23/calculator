#include <gtest/gtest.h>

#include "ast/binary.h"
#include "ast/equation.h"
#include "ast/function_call.h"
#include "ast/number.h"
#include "ast/variable.h"
#include "eval/context.h"
#include "solve/domain.h"

using namespace math_solver;

// ─── DomainCollector ─────────────────────────────────────

TEST(DomainCollector, DivisionAddsConstraint) {
    // x / (x - 3)
    auto expr = std::make_unique<BinaryOp>(
        std::make_unique<Variable>("x"),
        std::make_unique<BinaryOp>(std::make_unique<Variable>("x"),
                                   std::make_unique<Number>(3.0),
                                   BinaryOpType::Sub),
        BinaryOpType::Div);

    DomainCollector dc;
    dc.collect(*expr);

    ASSERT_EQ(dc.constraints().size(), 1u);
    EXPECT_EQ(dc.constraints()[0].kind, ConstraintKind::DivByZero);
}

TEST(DomainCollector, SqrtAddsConstraint) {
    // sqrt(x - 2)
    auto expr = std::make_unique<FunctionCall>(
        "sqrt", std::make_unique<BinaryOp>(std::make_unique<Variable>("x"),
                                           std::make_unique<Number>(2.0),
                                           BinaryOpType::Sub));

    DomainCollector dc;
    dc.collect(*expr);

    ASSERT_EQ(dc.constraints().size(), 1u);
    EXPECT_EQ(dc.constraints()[0].kind, ConstraintKind::SqrtNeg);
}

TEST(DomainCollector, LogAddsConstraint) {
    // ln(x)
    auto expr =
        std::make_unique<FunctionCall>("ln", std::make_unique<Variable>("x"));

    DomainCollector dc;
    dc.collect(*expr);

    ASSERT_EQ(dc.constraints().size(), 1u);
    EXPECT_EQ(dc.constraints()[0].kind, ConstraintKind::LogNonPos);
}

TEST(DomainCollector, MultipleConstraints) {
    // sqrt(x - 2) + 1/(x - 3)
    auto sqrt_expr = std::make_unique<FunctionCall>(
        "sqrt", std::make_unique<BinaryOp>(std::make_unique<Variable>("x"),
                                           std::make_unique<Number>(2.0),
                                           BinaryOpType::Sub));
    auto div_expr = std::make_unique<BinaryOp>(
        std::make_unique<Number>(1.0),
        std::make_unique<BinaryOp>(std::make_unique<Variable>("x"),
                                   std::make_unique<Number>(3.0),
                                   BinaryOpType::Sub),
        BinaryOpType::Div);
    auto expr = std::make_unique<BinaryOp>(
        std::move(sqrt_expr), std::move(div_expr), BinaryOpType::Add);

    DomainCollector dc;
    dc.collect(*expr);

    ASSERT_EQ(dc.constraints().size(), 2u);
}

TEST(DomainCollector, NoConstraints) {
    // x^2 + 3*x + 1
    auto expr = std::make_unique<BinaryOp>(
        std::make_unique<BinaryOp>(
            std::make_unique<BinaryOp>(std::make_unique<Variable>("x"),
                                       std::make_unique<Number>(2.0),
                                       BinaryOpType::Pow),
            std::make_unique<BinaryOp>(std::make_unique<Number>(3.0),
                                       std::make_unique<Variable>("x"),
                                       BinaryOpType::Mul),
            BinaryOpType::Add),
        std::make_unique<Number>(1.0), BinaryOpType::Add);

    DomainCollector dc;
    dc.collect(*expr);

    EXPECT_TRUE(dc.constraints().empty());
}

// ─── validate_root ────────────────────────────────────────

TEST(ValidateRoot, DivByZeroRejects) {
    // x / (x - 3) : denominator = x - 3
    auto denom = std::make_unique<BinaryOp>(std::make_unique<Variable>("x"),
                                            std::make_unique<Number>(3.0),
                                            BinaryOpType::Sub);

    std::vector<DomainConstraint> constraints;
    constraints.push_back(
        {ConstraintKind::DivByZero, denom.get(), "denom != 0"});

    // x = 3 should be rejected
    std::string reason = validate_root(constraints, "x", 3.0, nullptr, "");
    EXPECT_FALSE(reason.empty());

    // x = 5 should be accepted
    reason = validate_root(constraints, "x", 5.0, nullptr, "");
    EXPECT_TRUE(reason.empty());
}

TEST(ValidateRoot, SqrtNegRejects) {
    // sqrt(x - 2) : arg = x - 2
    auto arg = std::make_unique<BinaryOp>(std::make_unique<Variable>("x"),
                                          std::make_unique<Number>(2.0),
                                          BinaryOpType::Sub);

    std::vector<DomainConstraint> constraints;
    constraints.push_back(
        {ConstraintKind::SqrtNeg, arg.get(), "sqrt arg >= 0"});

    // x = 1 should be rejected (1 - 2 = -1 < 0)
    std::string reason = validate_root(constraints, "x", 1.0, nullptr, "");
    EXPECT_FALSE(reason.empty());

    // x = 5 should be accepted (5 - 2 = 3 >= 0)
    reason = validate_root(constraints, "x", 5.0, nullptr, "");
    EXPECT_TRUE(reason.empty());
}

// ─── collect_domain (convenience) ─────────────────────────

TEST(CollectDomain, BothSides) {
    // lhs = sqrt(x), rhs = 1/(x-1)
    auto lhs =
        std::make_unique<FunctionCall>("sqrt", std::make_unique<Variable>("x"));
    auto rhs = std::make_unique<BinaryOp>(
        std::make_unique<Number>(1.0),
        std::make_unique<BinaryOp>(std::make_unique<Variable>("x"),
                                   std::make_unique<Number>(1.0),
                                   BinaryOpType::Sub),
        BinaryOpType::Div);

    auto constraints = collect_domain(*lhs, *rhs);
    ASSERT_EQ(constraints.size(), 2u);
}
