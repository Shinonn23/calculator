#include <gtest/gtest.h>

#include "ast/command/var_command.hpp"
#include "lexer/command/command_token_stream.hpp"
#include "parser/command/subparsers/var_command_parser.hpp"

using namespace math_solver;

// ============================================================
// :set — action type
// ============================================================

TEST(VarCommandParser, SetAction) {
    VarCommandParser   parser;
    CommandTokenStream ts(":set x");
    auto               r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    EXPECT_EQ(static_cast<const VarCommand&>(*r->get()).action(),
              VarCommand::Action::Set);
}

TEST(VarCommandParser, UnsetAction) {
    VarCommandParser   parser;
    CommandTokenStream ts(":unset x");
    auto               r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    EXPECT_EQ(static_cast<const VarCommand&>(*r->get()).action(),
              VarCommand::Action::Unset);
}

TEST(VarCommandParser, UnsetMultipleVarsAction) {
    std::vector<std::string> var_names = {"x", "y", "z"};
    VarCommandParser         parser;
    CommandTokenStream       ts(":unset x, y, z");
    auto                     r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    EXPECT_EQ(static_cast<const VarCommand&>(*r->get()).var_name(), var_names);
}

// ============================================================
// Variable names
// ============================================================

TEST(VarCommandParser, SingleVarName) {
    VarCommandParser   parser;
    CommandTokenStream ts(":set myVar");
    auto               r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    auto& cmd = static_cast<const VarCommand&>(*r->get());
    ASSERT_EQ(cmd.var_name().size(), 1u);
    EXPECT_EQ(cmd.var_name()[0], "myVar");
}

TEST(VarCommandParser, UnsetSingleVarName) {
    VarCommandParser   parser;
    CommandTokenStream ts(":unset result");
    auto               r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    auto& cmd = static_cast<const VarCommand&>(*r->get());
    ASSERT_EQ(cmd.var_name().size(), 1u);
    EXPECT_EQ(cmd.var_name()[0], "result");
}

TEST(VarCommandParser, SetEmptyYieldsEmptyVarList) {
    VarCommandParser   parser;
    CommandTokenStream ts(":set");
    auto               r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    EXPECT_EQ(static_cast<const VarCommand&>(*r->get()).var_name().size(), 0u);
}

TEST(VarCommandParser, UnsetEmptyYieldsEmptyVarList) {
    VarCommandParser   parser;
    CommandTokenStream ts(":unset");
    auto               r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    auto& cmd = static_cast<const VarCommand&>(*r->get());
    EXPECT_EQ(cmd.action(), VarCommand::Action::Unset);
    EXPECT_EQ(cmd.var_name().size(), 0u);
}

// ============================================================
// :set — plain payload (no math action keyword)
// ============================================================

TEST(VarCommandParser, PlainNumericPayload) {
    VarCommandParser   parser;
    CommandTokenStream ts(":set x 42");
    auto               r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    auto& cmd = static_cast<const VarCommand&>(*r->get());
    EXPECT_TRUE(cmd.has_payload());
    // has_math_action() is true whenever set_payload() was called
    // (even with empty math_action string); check value instead
    EXPECT_EQ(cmd.payload(), "42");
    EXPECT_EQ(cmd.math_action(), "");
}

TEST(VarCommandParser, PlainExpressionPayload) {
    VarCommandParser   parser;
    CommandTokenStream ts(":set result 2*x + 1");
    auto               r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    EXPECT_EQ(static_cast<const VarCommand&>(*r->get()).payload(), "2*x + 1");
}

TEST(VarCommandParser, QuotedPayload) {
    VarCommandParser   parser;
    CommandTokenStream ts(":set x \"x + 1\"");
    auto               r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    auto& cmd = static_cast<const VarCommand&>(*r->get());
    EXPECT_TRUE(cmd.has_payload());
    EXPECT_EQ(cmd.payload(), "x + 1");
}

TEST(VarCommandParser, QuotedPayloadWithParens) {
    VarCommandParser   parser;
    CommandTokenStream ts(":set p \"(x+1)^2\"");
    auto               r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    EXPECT_EQ(static_cast<const VarCommand&>(*r->get()).payload(), "(x+1)^2");
}

TEST(VarCommandParser, NonMathKeywordIsPayload) {
    // "simplify" is not one of solve/expand/factor → plain payload
    VarCommandParser   parser;
    CommandTokenStream ts(":set x simplify");
    auto               r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    auto& cmd = static_cast<const VarCommand&>(*r->get());
    // has_math_action() is true whenever set_payload() was called;
    // check math_action() value is "" for non-math-keyword payloads
    EXPECT_EQ(cmd.math_action(), "");
    EXPECT_EQ(cmd.payload(), "simplify");
}

TEST(VarCommandParser, SetVarNoPayloadYieldsEmpty) {
    VarCommandParser   parser;
    CommandTokenStream ts(":set x");
    auto               r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    EXPECT_EQ(static_cast<const VarCommand&>(*r->get()).payload(), "");
}

// ============================================================
// :set — math action keywords (solve / expand / factor)
// ============================================================

TEST(VarCommandParser, SolveMathAction) {
    VarCommandParser   parser;
    CommandTokenStream ts(":set root solve x + 3 = 0");
    auto               r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    auto& cmd = static_cast<const VarCommand&>(*r->get());
    EXPECT_TRUE(cmd.has_math_action());
    EXPECT_EQ(cmd.math_action(), "solve");
    EXPECT_EQ(cmd.payload(), "x + 3 = 0");
}

TEST(VarCommandParser, ExpandMathAction) {
    VarCommandParser   parser;
    CommandTokenStream ts(":set p expand (x+1)^2");
    auto               r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    auto& cmd = static_cast<const VarCommand&>(*r->get());
    EXPECT_EQ(cmd.math_action(), "expand");
    EXPECT_EQ(cmd.payload(), "(x+1)^2");
}

TEST(VarCommandParser, FactorMathAction) {
    VarCommandParser   parser;
    CommandTokenStream ts(":set f factor x^2 - 4");
    auto               r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    auto& cmd = static_cast<const VarCommand&>(*r->get());
    EXPECT_EQ(cmd.math_action(), "factor");
    EXPECT_EQ(cmd.payload(), "x^2 - 4");
}

TEST(VarCommandParser, SolveMathActionQuotedExpr) {
    VarCommandParser   parser;
    CommandTokenStream ts(":set root solve \"x^2 - 4 = 0\"");
    auto               r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    auto& cmd = static_cast<const VarCommand&>(*r->get());
    EXPECT_EQ(cmd.math_action(), "solve");
    EXPECT_EQ(cmd.payload(), "x^2 - 4 = 0");
}

TEST(VarCommandParser, ExpandMathActionQuotedExpr) {
    VarCommandParser   parser;
    CommandTokenStream ts(":set p expand \"(x+1)^2\"");
    auto               r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    auto& cmd = static_cast<const VarCommand&>(*r->get());
    EXPECT_EQ(cmd.math_action(), "expand");
    EXPECT_EQ(cmd.payload(), "(x+1)^2");
}

TEST(VarCommandParser, FactorMathActionQuotedExpr) {
    VarCommandParser   parser;
    CommandTokenStream ts(":set f factor \"x^2 - 4\"");
    auto               r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    auto& cmd = static_cast<const VarCommand&>(*r->get());
    EXPECT_EQ(cmd.math_action(), "factor");
    EXPECT_EQ(cmd.payload(), "x^2 - 4");
}

// ============================================================
// :unset — no payload branch
// ============================================================

TEST(VarCommandParser, UnsetHasNoPayload) {
    VarCommandParser   parser;
    CommandTokenStream ts(":unset x");
    auto               r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    EXPECT_FALSE(static_cast<const VarCommand&>(*r->get()).has_payload());
}

// ============================================================
// Error: trailing flag after unquoted expression
// ============================================================

TEST(VarCommandParser, TrailingFlagAfterPlainPayload) {
    VarCommandParser   parser;
    CommandTokenStream ts(":set x x + 1 --no-save");
    EXPECT_FALSE(parser.parse(ts).ok());
}

TEST(VarCommandParser, TrailingFlagAfterSolvePayload) {
    VarCommandParser   parser;
    CommandTokenStream ts(":set r solve x = 0 --no-save");
    EXPECT_FALSE(parser.parse(ts).ok());
}

TEST(VarCommandParser, TrailingFlagAfterExpandPayload) {
    VarCommandParser   parser;
    CommandTokenStream ts(":set p expand (x+1)^2 --isolated");
    EXPECT_FALSE(parser.parse(ts).ok());
}
