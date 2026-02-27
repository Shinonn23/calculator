#include <gtest/gtest.h>

#include "ast/command/history_command.hpp"
#include "ast/mock_visitors.hpp"
#include "diagnostics/sink.hpp"

using namespace math_solver;
using test_helpers::MockCommandVisitor;

// ── Action ────────────────────────────────────────────────────────────────

TEST(HistoryCommandTest, AllActionsConstructible) {
    EXPECT_EQ(HistoryCommand(HistoryCommand::Action::Show,      "").action(), HistoryCommand::Action::Show);
    EXPECT_EQ(HistoryCommand(HistoryCommand::Action::ShowRange, "").action(), HistoryCommand::Action::ShowRange);
    EXPECT_EQ(HistoryCommand(HistoryCommand::Action::Search,    "").action(), HistoryCommand::Action::Search);
    EXPECT_EQ(HistoryCommand(HistoryCommand::Action::Save,      "").action(), HistoryCommand::Action::Save);
    EXPECT_EQ(HistoryCommand(HistoryCommand::Action::Clear,     "").action(), HistoryCommand::Action::Clear);
    EXPECT_EQ(HistoryCommand(HistoryCommand::Action::Unknown,   "").action(), HistoryCommand::Action::Unknown);
}

// ── limit ────────────────────────────────────────────────────────────────

TEST(HistoryCommandTest, LimitDefaultIs20) {
    EXPECT_EQ(HistoryCommand(HistoryCommand::Action::Show, "history").limit(), 20);
}

TEST(HistoryCommandTest, SetLimit) {
    HistoryCommand c(HistoryCommand::Action::Show, "history 5");
    c.set_limit(5);
    EXPECT_EQ(c.limit(), 5);
}

// ── pattern / filepath / range ───────────────────────────────────────────

TEST(HistoryCommandTest, SetPattern) {
    HistoryCommand c(HistoryCommand::Action::Search, "history search foo");
    c.set_pattern("foo");
    EXPECT_EQ(c.pattern(), "foo");
}

TEST(HistoryCommandTest, SetFilepath) {
    HistoryCommand c(HistoryCommand::Action::Save, "history save out.txt");
    c.set_filepath("out.txt");
    EXPECT_EQ(c.filepath(), "out.txt");
}

TEST(HistoryCommandTest, SetRange) {
    HistoryCommand c(HistoryCommand::Action::ShowRange, "history 1-3");
    c.set_range({1, 2, 3});
    ASSERT_EQ(c.range().size(), 3u);
    EXPECT_EQ(c.range()[0], 1);
    EXPECT_EQ(c.range()[2], 3);
}

// ── is_readonly ──────────────────────────────────────────────────────────

TEST(HistoryCommandTest, ReadonlyActions) {
    EXPECT_TRUE(HistoryCommand(HistoryCommand::Action::Show,      "").is_readonly());
    EXPECT_TRUE(HistoryCommand(HistoryCommand::Action::ShowRange, "").is_readonly());
    EXPECT_TRUE(HistoryCommand(HistoryCommand::Action::Search,    "").is_readonly());
}

TEST(HistoryCommandTest, MutatingActionsNotReadonly) {
    EXPECT_FALSE(HistoryCommand(HistoryCommand::Action::Save,    "").is_readonly());
    EXPECT_FALSE(HistoryCommand(HistoryCommand::Action::Clear,   "").is_readonly());
    EXPECT_FALSE(HistoryCommand(HistoryCommand::Action::Unknown, "").is_readonly());
}

// ── flags ────────────────────────────────────────────────────────────────

TEST(HistoryCommandTest, FlagsEmptyByDefault) {
    HistoryCommand c(HistoryCommand::Action::Show, "history");
    EXPECT_FALSE(c.has_any_flag());
    EXPECT_FALSE(c.has_flag(HistoryCommand::Flag::Errors));
}

TEST(HistoryCommandTest, SetSingleFlag) {
    HistoryCommand c(HistoryCommand::Action::Show, "history");
    c.set_flags({HistoryCommand::Flag::Errors});
    EXPECT_TRUE(c.has_any_flag());
    EXPECT_TRUE(c.has_flag(HistoryCommand::Flag::Errors));
    EXPECT_FALSE(c.has_flag(HistoryCommand::Flag::Success));
}

TEST(HistoryCommandTest, SetMultipleFlags) {
    HistoryCommand c(HistoryCommand::Action::Show, "history");
    c.set_flags({HistoryCommand::Flag::Errors, HistoryCommand::Flag::Warning});
    EXPECT_TRUE(c.has_flag(HistoryCommand::Flag::Errors));
    EXPECT_TRUE(c.has_flag(HistoryCommand::Flag::Warning));
    EXPECT_FALSE(c.has_flag(HistoryCommand::Flag::Success));
    EXPECT_FALSE(c.has_flag(HistoryCommand::Flag::Info));
}

TEST(HistoryCommandTest, SetFlagsReplacesExisting) {
    HistoryCommand c(HistoryCommand::Action::Show, "history");
    c.set_flags({HistoryCommand::Flag::Errors});
    c.set_flags({HistoryCommand::Flag::Success});
    EXPECT_FALSE(c.has_flag(HistoryCommand::Flag::Errors));
    EXPECT_TRUE(c.has_flag(HistoryCommand::Flag::Success));
}

TEST(HistoryCommandTest, AcceptDispatchesHistoryCommand) {
    HistoryCommand     c(HistoryCommand::Action::Show, "history");
    MockCommandVisitor v;
    DiagnosticSink     sink;
    c.accept(v, sink);
    EXPECT_EQ(v.last, MockCommandVisitor::Visited::History);
}
