#include <gtest/gtest.h>

#include "ast/command/load_command.hpp"
#include "lexer/command/command_token_stream.hpp"
#include "parser/command/subparsers/load_command_parser.hpp"

using namespace math_solver;

// ============================================================
// Basic filepath parsing
// ============================================================

TEST(LoadCommandParser, SimpleFilepath) {
    LoadCommandParser  parser;
    CommandTokenStream ts(":load script.msl");
    auto r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    EXPECT_EQ(static_cast<const LoadCommand&>(*r->get()).filepath(),
              "script.msl");
}

TEST(LoadCommandParser, FilepathWithSubdir) {
    LoadCommandParser  parser;
    CommandTokenStream ts(":load tests/ui/math_solve.msl");
    auto r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    EXPECT_EQ(static_cast<const LoadCommand&>(*r->get()).filepath(),
              "tests/ui/math_solve.msl");
}

// ============================================================
// Flags
// ============================================================

TEST(LoadCommandParser, DryRunFlag) {
    LoadCommandParser  parser;
    CommandTokenStream ts(":load --dry-run script.msl");
    auto r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    auto& cmd = static_cast<const LoadCommand&>(*r->get());
    EXPECT_TRUE(cmd.flags().dry_run);
    EXPECT_EQ(cmd.filepath(), "script.msl");
}

TEST(LoadCommandParser, SilentFlag) {
    LoadCommandParser  parser;
    CommandTokenStream ts(":load --silent script.msl");
    auto r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    EXPECT_TRUE(static_cast<const LoadCommand&>(*r->get()).flags().silent);
}

TEST(LoadCommandParser, StrictFlag) {
    LoadCommandParser  parser;
    CommandTokenStream ts(":load --strict script.msl");
    auto r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    EXPECT_TRUE(static_cast<const LoadCommand&>(*r->get()).flags().strict);
}

TEST(LoadCommandParser, NoRollbackFlag) {
    LoadCommandParser  parser;
    CommandTokenStream ts(":load --no-rollback script.msl");
    auto r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    EXPECT_TRUE(static_cast<const LoadCommand&>(*r->get()).flags().no_rollback);
}

TEST(LoadCommandParser, EnvFlag) {
    LoadCommandParser  parser;
    CommandTokenStream ts(":load --env production script.msl");
    auto r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    auto& cmd = static_cast<const LoadCommand&>(*r->get());
    EXPECT_EQ(cmd.flags().env, "production");
    EXPECT_EQ(cmd.filepath(), "script.msl");
}

TEST(LoadCommandParser, MultipleFlags) {
    LoadCommandParser  parser;
    CommandTokenStream ts(":load --dry-run --silent script.msl");
    auto r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    auto& cmd = static_cast<const LoadCommand&>(*r->get());
    EXPECT_TRUE(cmd.flags().dry_run);
    EXPECT_TRUE(cmd.flags().silent);
}

TEST(LoadCommandParser, FlagAfterFilepath) {
    LoadCommandParser  parser;
    CommandTokenStream ts(":load script.msl --silent");
    auto r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    auto& cmd = static_cast<const LoadCommand&>(*r->get());
    EXPECT_EQ(cmd.filepath(), "script.msl");
    EXPECT_TRUE(cmd.flags().silent);
}

// ============================================================
// Default flags (all false, env empty)
// ============================================================

TEST(LoadCommandParser, DefaultFlagsAllFalse) {
    LoadCommandParser  parser;
    CommandTokenStream ts(":load script.msl");
    auto r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    auto& f = static_cast<const LoadCommand&>(*r->get()).flags();
    EXPECT_FALSE(f.dry_run);
    EXPECT_FALSE(f.silent);
    EXPECT_FALSE(f.strict);
    EXPECT_FALSE(f.no_rollback);
    EXPECT_EQ(f.env, "");
}

// ============================================================
// Error cases
// ============================================================

TEST(LoadCommandParser, NoFilepathIsError) {
    LoadCommandParser  parser;
    CommandTokenStream ts(":load");
    EXPECT_FALSE(parser.parse(ts).ok());
}

TEST(LoadCommandParser, OnlyFlagsNoFilepathIsError) {
    LoadCommandParser  parser;
    CommandTokenStream ts(":load --dry-run --silent");
    EXPECT_FALSE(parser.parse(ts).ok());
}

TEST(LoadCommandParser, UnknownFlagIsError) {
    LoadCommandParser  parser;
    CommandTokenStream ts(":load --unknown script.msl");
    EXPECT_FALSE(parser.parse(ts).ok());
}

TEST(LoadCommandParser, ExtraPositionalArgIsError) {
    LoadCommandParser  parser;
    CommandTokenStream ts(":load script.msl extra");
    EXPECT_FALSE(parser.parse(ts).ok());
}

TEST(LoadCommandParser, EnvFlagWithoutArgIsError) {
    LoadCommandParser  parser;
    CommandTokenStream ts(":load --env");
    EXPECT_FALSE(parser.parse(ts).ok());
}
