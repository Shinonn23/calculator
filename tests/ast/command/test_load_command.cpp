#include <gtest/gtest.h>

#include "ast/command/load_command.hpp"
#include "ast/mock_visitors.hpp"
#include "diagnostics/sink.hpp"

using namespace math_solver;
using test_helpers::MockCommandVisitor;

TEST(LoadCommandTest, FilepathStored) {
    LoadCommand c("script.msl", {}, "load script.msl");
    EXPECT_EQ(c.filepath(), "script.msl");
}

TEST(LoadCommandTest, FlagsAllDefaultFalse) {
    LoadCommand c("f.msl", {}, "load f.msl");
    EXPECT_FALSE(c.flags().dry_run);
    EXPECT_FALSE(c.flags().silent);
    EXPECT_FALSE(c.flags().strict);
    EXPECT_FALSE(c.flags().no_rollback);
    EXPECT_TRUE(c.flags().env.empty());
}

TEST(LoadCommandTest, FlagsAllSet) {
    LoadCommand::Flags f{true, true, true, true, "myenv"};
    LoadCommand        c("f.msl", f, "load f.msl");
    EXPECT_TRUE(c.flags().dry_run);
    EXPECT_TRUE(c.flags().silent);
    EXPECT_TRUE(c.flags().strict);
    EXPECT_TRUE(c.flags().no_rollback);
    EXPECT_EQ(c.flags().env, "myenv");
}

TEST(LoadCommandTest, FlagsPartiallySet) {
    LoadCommand::Flags f;
    f.dry_run = true;
    f.env     = "test_env";
    LoadCommand c("f.msl", f, "load f.msl");
    EXPECT_TRUE(c.flags().dry_run);
    EXPECT_FALSE(c.flags().silent);
    EXPECT_EQ(c.flags().env, "test_env");
}

TEST(LoadCommandTest, AcceptDispatchesLoadCommand) {
    LoadCommand        c("f.msl", {}, "load f.msl");
    MockCommandVisitor v;
    DiagnosticSink     sink;
    c.accept(v, sink);
    EXPECT_EQ(v.last, MockCommandVisitor::Visited::Load);
}
