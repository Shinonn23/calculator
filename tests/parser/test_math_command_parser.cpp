#include <gtest/gtest.h>

#include "ast/command/math_command.hpp"
#include "lexer/command/command_token_stream.hpp"
#include "parser/command/subparsers/math_command_parser.hpp"

using namespace math_solver;

// ============================================================
// Command type resolution
// ============================================================

TEST(MathCommandParser, SolveType) {
    MathCommandParser  parser;
    CommandTokenStream ts(":solve x + 1 = 0");
    auto r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    EXPECT_EQ(static_cast<const MathCommand&>(*r->get()).type(),
              MathCommand::Type::Solve);
}

TEST(MathCommandParser, SimplifyType) {
    MathCommandParser  parser;
    CommandTokenStream ts(":simplify x + 0");
    auto r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    EXPECT_EQ(static_cast<const MathCommand&>(*r->get()).type(),
              MathCommand::Type::Simplify);
}

TEST(MathCommandParser, ExpandType) {
    MathCommandParser  parser;
    CommandTokenStream ts(":expand (x+1)^2");
    auto r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    EXPECT_EQ(static_cast<const MathCommand&>(*r->get()).type(),
              MathCommand::Type::Expand);
}

TEST(MathCommandParser, FactorType) {
    MathCommandParser  parser;
    CommandTokenStream ts(":factor x^2 - 4");
    auto r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    EXPECT_EQ(static_cast<const MathCommand&>(*r->get()).type(),
              MathCommand::Type::Factor);
}

// ============================================================
// Payload extraction
// ============================================================

TEST(MathCommandParser, UnquotedPayload) {
    MathCommandParser  parser;
    CommandTokenStream ts(":solve x + 1 = 0");
    auto r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    EXPECT_EQ(static_cast<const MathCommand&>(*r->get()).payload(),
              "x + 1 = 0");
}

TEST(MathCommandParser, QuotedPayload) {
    MathCommandParser  parser;
    CommandTokenStream ts(":solve \"x + 1 = 0\"");
    auto r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    EXPECT_EQ(static_cast<const MathCommand&>(*r->get()).payload(),
              "x + 1 = 0");
}

TEST(MathCommandParser, EmptyPayload) {
    MathCommandParser  parser;
    CommandTokenStream ts(":solve");
    auto r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    EXPECT_EQ(static_cast<const MathCommand&>(*r->get()).payload(), "");
}

TEST(MathCommandParser, MultiwordPayload) {
    MathCommandParser  parser;
    CommandTokenStream ts(":expand (x + y)^2");
    auto r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    EXPECT_EQ(static_cast<const MathCommand&>(*r->get()).payload(),
              "(x + y)^2");
}

// ============================================================
// Default flags (all false / default values)
// ============================================================

TEST(MathCommandParser, AllDefaultsFalse) {
    MathCommandParser  parser;
    CommandTokenStream ts(":solve x = 0");
    auto r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    auto& cmd = static_cast<const MathCommand&>(*r->get());
    EXPECT_FALSE(cmd.isolated());
    EXPECT_FALSE(cmd.as_fraction());
    EXPECT_FALSE(cmd.no_save());
    EXPECT_FALSE(cmd.show_matrix());
    EXPECT_FALSE(cmd.show_rank());
    EXPECT_FALSE(cmd.detect_singular());
    EXPECT_FALSE(cmd.free_vars());
    EXPECT_FALSE(cmd.method_explicitly_set());
    EXPECT_EQ(cmd.method(), SolveMethod::Gauss);
    EXPECT_TRUE(cmd.specific_vars().empty());
}

// ============================================================
// Boolean flags — before unquoted expression
// ============================================================

TEST(MathCommandParser, FlagIsolatedLong) {
    MathCommandParser  parser;
    CommandTokenStream ts(":solve --isolated x = 1");
    auto r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    EXPECT_TRUE(static_cast<const MathCommand&>(*r->get()).isolated());
}

TEST(MathCommandParser, FlagIsolatedShort) {
    MathCommandParser  parser;
    CommandTokenStream ts(":solve -isolated x = 1");
    auto r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    EXPECT_TRUE(static_cast<const MathCommand&>(*r->get()).isolated());
}

TEST(MathCommandParser, FlagFraction) {
    MathCommandParser  parser;
    CommandTokenStream ts(":solve --fraction x = 1");
    auto r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    EXPECT_TRUE(static_cast<const MathCommand&>(*r->get()).as_fraction());
}

TEST(MathCommandParser, FlagExactAliasFraction) {
    MathCommandParser  parser;
    CommandTokenStream ts(":solve --exact x = 1");
    auto r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    EXPECT_TRUE(static_cast<const MathCommand&>(*r->get()).as_fraction());
}

TEST(MathCommandParser, FlagNoSave) {
    MathCommandParser  parser;
    CommandTokenStream ts(":solve --no-save x = 1");
    auto r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    EXPECT_TRUE(static_cast<const MathCommand&>(*r->get()).no_save());
}

TEST(MathCommandParser, FlagShowMatrix) {
    MathCommandParser  parser;
    CommandTokenStream ts(":solve --show-matrix x = 1");
    auto r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    EXPECT_TRUE(static_cast<const MathCommand&>(*r->get()).show_matrix());
}

TEST(MathCommandParser, FlagRank) {
    MathCommandParser  parser;
    CommandTokenStream ts(":solve --rank x = 1");
    auto r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    EXPECT_TRUE(static_cast<const MathCommand&>(*r->get()).show_rank());
}

TEST(MathCommandParser, FlagDetectSingular) {
    MathCommandParser  parser;
    CommandTokenStream ts(":solve --detect-singular x = 1");
    auto r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    EXPECT_TRUE(static_cast<const MathCommand&>(*r->get()).detect_singular());
}

TEST(MathCommandParser, FlagFreeVars) {
    MathCommandParser  parser;
    CommandTokenStream ts(":solve --free-vars x = 1");
    auto r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    EXPECT_TRUE(static_cast<const MathCommand&>(*r->get()).free_vars());
}

// ============================================================
// --method flag
// ============================================================

TEST(MathCommandParser, MethodDefaultIsGauss) {
    MathCommandParser  parser;
    CommandTokenStream ts(":solve x = 1");
    auto r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    auto& cmd = static_cast<const MathCommand&>(*r->get());
    EXPECT_EQ(cmd.method(), SolveMethod::Gauss);
    EXPECT_FALSE(cmd.method_explicitly_set());
}

TEST(MathCommandParser, MethodLU) {
    MathCommandParser  parser;
    CommandTokenStream ts(":solve --method=lu x = 1");
    auto r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    auto& cmd = static_cast<const MathCommand&>(*r->get());
    EXPECT_EQ(cmd.method(), SolveMethod::LU);
    EXPECT_TRUE(cmd.method_explicitly_set());
}

TEST(MathCommandParser, MethodGaussExplicit) {
    MathCommandParser  parser;
    CommandTokenStream ts(":solve --method=gauss x = 1");
    auto r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    auto& cmd = static_cast<const MathCommand&>(*r->get());
    EXPECT_EQ(cmd.method(), SolveMethod::Gauss);
    EXPECT_TRUE(cmd.method_explicitly_set());
}

// ============================================================
// Multiple flags combined
// ============================================================

TEST(MathCommandParser, MultipleFlags) {
    MathCommandParser  parser;
    CommandTokenStream ts(":solve --no-save --fraction x = 1");
    auto r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    auto& cmd = static_cast<const MathCommand&>(*r->get());
    EXPECT_TRUE(cmd.no_save());
    EXPECT_TRUE(cmd.as_fraction());
}

TEST(MathCommandParser, FlagsAndPayloadBothSet) {
    MathCommandParser  parser;
    CommandTokenStream ts(":solve --isolated --no-save x + 1 = 0");
    auto r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    auto& cmd = static_cast<const MathCommand&>(*r->get());
    EXPECT_TRUE(cmd.isolated());
    EXPECT_TRUE(cmd.no_save());
    EXPECT_EQ(cmd.payload(), "x + 1 = 0");
}

// ============================================================
// Flags after a QUOTED expression (phase-3 flags)
// ============================================================

TEST(MathCommandParser, FlagAfterQuotedExpr) {
    MathCommandParser  parser;
    CommandTokenStream ts(":solve \"x + 1 = 0\" --no-save");
    auto r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    auto& cmd = static_cast<const MathCommand&>(*r->get());
    EXPECT_EQ(cmd.payload(), "x + 1 = 0");
    EXPECT_TRUE(cmd.no_save());
}

TEST(MathCommandParser, MultipleFlagsAfterQuotedExpr) {
    MathCommandParser  parser;
    CommandTokenStream ts(":solve \"x = 1\" --no-save --fraction");
    auto r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    auto& cmd = static_cast<const MathCommand&>(*r->get());
    EXPECT_TRUE(cmd.no_save());
    EXPECT_TRUE(cmd.as_fraction());
}

TEST(MathCommandParser, FlagBeforeAndAfterQuotedExpr) {
    MathCommandParser  parser;
    CommandTokenStream ts(":solve --isolated \"x = 1\" --no-save");
    auto r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    auto& cmd = static_cast<const MathCommand&>(*r->get());
    EXPECT_TRUE(cmd.isolated());
    EXPECT_TRUE(cmd.no_save());
    EXPECT_EQ(cmd.payload(), "x = 1");
}

// ============================================================
// --vars flag
// ============================================================

TEST(MathCommandParser, VarsFlagSingleVar) {
    // --vars greedily consumes all Word+QuotedString tokens after it.
    // With `:solve --vars x "expr"`, both "x" and "expr" go into vars.
    MathCommandParser  parser;
    CommandTokenStream ts(":solve --vars x \"x + 1 = 0\"");
    auto r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    auto& cmd = static_cast<const MathCommand&>(*r->get());
    ASSERT_EQ(cmd.specific_vars().size(), 2u); // "x" and "x + 1 = 0"
    EXPECT_EQ(cmd.specific_vars()[0], "x");
}

TEST(MathCommandParser, VarsFlagMultipleVars) {
    // Three tokens after --vars: "x", "y", and the quoted expr
    MathCommandParser  parser;
    CommandTokenStream ts(":solve --vars x y \"x + y = 1\"");
    auto r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    auto& cmd = static_cast<const MathCommand&>(*r->get());
    ASSERT_EQ(cmd.specific_vars().size(), 3u); // "x", "y", "x + y = 1"
    EXPECT_EQ(cmd.specific_vars()[0], "x");
    EXPECT_EQ(cmd.specific_vars()[1], "y");
}

TEST(MathCommandParser, VarsFlagShortForm) {
    // -vars greedily consumes "x" and the quoted expr
    MathCommandParser  parser;
    CommandTokenStream ts(":solve -vars x \"x = 0\"");
    auto r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    EXPECT_EQ(static_cast<const MathCommand&>(*r->get()).specific_vars().size(),
              2u); // "x" and "x = 0"
}

TEST(MathCommandParser, VarsFlagWordsOnly) {
    // With no quoted expr, --vars consumes only the word tokens
    MathCommandParser  parser;
    CommandTokenStream ts(":solve --vars x y");
    auto r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    auto& cmd = static_cast<const MathCommand&>(*r->get());
    ASSERT_EQ(cmd.specific_vars().size(), 2u);
    EXPECT_EQ(cmd.specific_vars()[0], "x");
    EXPECT_EQ(cmd.specific_vars()[1], "y");
}

// ============================================================
// Error: trailing flag in unquoted expression
// ============================================================

TEST(MathCommandParser, TrailingFlagInUnquotedSolveIsError) {
    MathCommandParser  parser;
    CommandTokenStream ts(":solve x + 1 = 0 --no-save");
    EXPECT_FALSE(parser.parse(ts).ok());
}

TEST(MathCommandParser, TrailingFlagInExpandIsError) {
    MathCommandParser  parser;
    CommandTokenStream ts(":expand (x+1)^2 --isolated");
    EXPECT_FALSE(parser.parse(ts).ok());
}

TEST(MathCommandParser, TrailingFlagInFactorIsError) {
    MathCommandParser  parser;
    CommandTokenStream ts(":factor x^2 - 4 --fraction");
    EXPECT_FALSE(parser.parse(ts).ok());
}
