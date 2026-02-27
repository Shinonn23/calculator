#include <gtest/gtest.h>

#include "ast/command/math_command.hpp"
#include "ast/mock_visitors.hpp"
#include "diagnostics/sink.hpp"

using namespace math_solver;
using test_helpers::MockCommandVisitor;

TEST(MathCommandTest, TypeStored) {
    EXPECT_EQ(MathCommand(MathCommand::Type::Solve, "x=0", "solve x=0").type(),
              MathCommand::Type::Solve);
}

TEST(MathCommandTest, PayloadStored) {
    MathCommand c(MathCommand::Type::Evaluate, "2+2", "eval 2+2");
    EXPECT_EQ(c.payload(), "2+2");
}

TEST(MathCommandTest, FlagsDefaultFalse) {
    MathCommand c(MathCommand::Type::Evaluate, "1", "eval 1");
    EXPECT_FALSE(c.isolated());
    EXPECT_FALSE(c.as_fraction());
    EXPECT_TRUE(c.specific_vars().empty());
}

TEST(MathCommandTest, SetFlagsIsolated) {
    MathCommand c(MathCommand::Type::Solve, "x=1", "solve x=1");
    c.set_flags(true, false);
    EXPECT_TRUE(c.isolated());
    EXPECT_FALSE(c.as_fraction());
}

TEST(MathCommandTest, SetFlagsFraction) {
    MathCommand c(MathCommand::Type::Simplify, "a/b", "simplify a/b");
    c.set_flags(false, true);
    EXPECT_FALSE(c.isolated());
    EXPECT_TRUE(c.as_fraction());
}

TEST(MathCommandTest, SetFlagsBoth) {
    MathCommand c(MathCommand::Type::Evaluate, "x", "eval x");
    c.set_flags(true, true);
    EXPECT_TRUE(c.isolated());
    EXPECT_TRUE(c.as_fraction());
}

TEST(MathCommandTest, SetFlagsSpecificVars) {
    MathCommand c(MathCommand::Type::Solve, "x+y=1", "solve x+y=1");
    c.set_flags(false, false, {"x", "y"});
    ASSERT_EQ(c.specific_vars().size(), 2u);
    EXPECT_EQ(c.specific_vars()[0], "x");
    EXPECT_EQ(c.specific_vars()[1], "y");
}

TEST(MathCommandTest, AllTypesConstructible) {
    for (auto t : {MathCommand::Type::Evaluate, MathCommand::Type::Solve,
                   MathCommand::Type::Simplify, MathCommand::Type::Expand,
                   MathCommand::Type::Factor,   MathCommand::Type::Unknown}) {
        EXPECT_EQ(MathCommand(t, "", "").type(), t);
    }
}

TEST(MathCommandTest, AcceptDispatchesMathCommand) {
    MathCommand        c(MathCommand::Type::Evaluate, "1", "eval 1");
    MockCommandVisitor v;
    DiagnosticSink     sink;
    c.accept(v, sink);
    EXPECT_EQ(v.last, MockCommandVisitor::Visited::Math);
}
