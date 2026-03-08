#include <gtest/gtest.h>

#include "ast/command/history_command.hpp"
#include "lexer/command/command_token_stream.hpp"
#include "parser/command/subparsers/history_command_parser.hpp"

using namespace math_solver;

// ============================================================
// Default action (no arguments)
// ============================================================

TEST(HistoryCommandParser, BareHistoryDefaultsShow) {
    HistoryCommandParser parser;
    CommandTokenStream   ts(":history");
    auto r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    auto& cmd = static_cast<const HistoryCommand&>(*r->get());
    EXPECT_EQ(cmd.action(), HistoryCommand::Action::Show);
    EXPECT_EQ(cmd.limit(), 20); // default limit
}

// ============================================================
// Show with explicit limit
// ============================================================

TEST(HistoryCommandParser, ShowWithLimit) {
    HistoryCommandParser parser;
    CommandTokenStream   ts(":history 5");
    auto r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    auto& cmd = static_cast<const HistoryCommand&>(*r->get());
    EXPECT_EQ(cmd.action(), HistoryCommand::Action::Show);
    EXPECT_EQ(cmd.limit(), 5);
}

TEST(HistoryCommandParser, ShowAllKeyword) {
    HistoryCommandParser parser;
    CommandTokenStream   ts(":history all");
    auto r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    auto& cmd = static_cast<const HistoryCommand&>(*r->get());
    EXPECT_EQ(cmd.action(), HistoryCommand::Action::Show);
    EXPECT_EQ(cmd.limit(), 0); // 0 means show all
}

// ============================================================
// ShowRange
// ============================================================

TEST(HistoryCommandParser, RangeWithDash) {
    HistoryCommandParser parser;
    CommandTokenStream   ts(":history 1-5");
    auto r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    auto& cmd = static_cast<const HistoryCommand&>(*r->get());
    EXPECT_EQ(cmd.action(), HistoryCommand::Action::ShowRange);
    ASSERT_EQ(cmd.range().size(), 5u);
    EXPECT_EQ(cmd.range()[0], 1);
    EXPECT_EQ(cmd.range()[4], 5);
}

TEST(HistoryCommandParser, RangeWithTwoArgs) {
    HistoryCommandParser parser;
    CommandTokenStream   ts(":history 3 7");
    auto r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    auto& cmd = static_cast<const HistoryCommand&>(*r->get());
    EXPECT_EQ(cmd.action(), HistoryCommand::Action::ShowRange);
    EXPECT_FALSE(cmd.range().empty());
    EXPECT_EQ(cmd.range().front(), 3);
    EXPECT_EQ(cmd.range().back(), 7);
}

TEST(HistoryCommandParser, RangeReversedIsNormalized) {
    // "7 3" should be treated as "3 7" (swapped)
    HistoryCommandParser parser;
    CommandTokenStream   ts(":history 7 3");
    auto r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    auto& cmd = static_cast<const HistoryCommand&>(*r->get());
    EXPECT_EQ(cmd.action(), HistoryCommand::Action::ShowRange);
    EXPECT_EQ(cmd.range().front(), 3);
    EXPECT_EQ(cmd.range().back(), 7);
}

// ============================================================
// Clear
// ============================================================

TEST(HistoryCommandParser, ClearAction) {
    HistoryCommandParser parser;
    CommandTokenStream   ts(":history clear");
    auto r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    EXPECT_EQ(static_cast<const HistoryCommand&>(*r->get()).action(),
              HistoryCommand::Action::Clear);
}

// ============================================================
// Search
// ============================================================

TEST(HistoryCommandParser, SearchWithPattern) {
    HistoryCommandParser parser;
    CommandTokenStream   ts(":history search solve");
    auto r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    auto& cmd = static_cast<const HistoryCommand&>(*r->get());
    EXPECT_EQ(cmd.action(), HistoryCommand::Action::Search);
    EXPECT_EQ(cmd.pattern(), "solve");
}

TEST(HistoryCommandParser, SearchWithMultiWordPattern) {
    HistoryCommandParser parser;
    CommandTokenStream   ts(":history search x + 1");
    auto r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    auto& cmd = static_cast<const HistoryCommand&>(*r->get());
    EXPECT_EQ(cmd.action(), HistoryCommand::Action::Search);
    EXPECT_FALSE(cmd.pattern().empty());
}

TEST(HistoryCommandParser, SearchEmptyPattern) {
    HistoryCommandParser parser;
    CommandTokenStream   ts(":history search");
    auto r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    EXPECT_EQ(static_cast<const HistoryCommand&>(*r->get()).action(),
              HistoryCommand::Action::Search);
}

// ============================================================
// Save
// ============================================================

TEST(HistoryCommandParser, SaveWithFilepath) {
    HistoryCommandParser parser;
    CommandTokenStream   ts(":history save out.txt");
    auto r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    auto& cmd = static_cast<const HistoryCommand&>(*r->get());
    EXPECT_EQ(cmd.action(), HistoryCommand::Action::Save);
    EXPECT_EQ(cmd.filepath(), "out.txt");
}

TEST(HistoryCommandParser, SaveWithFilepathAndRange) {
    HistoryCommandParser parser;
    CommandTokenStream   ts(":history save out.txt 1-3");
    auto r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    auto& cmd = static_cast<const HistoryCommand&>(*r->get());
    EXPECT_EQ(cmd.action(), HistoryCommand::Action::Save);
    EXPECT_EQ(cmd.filepath(), "out.txt");
    EXPECT_FALSE(cmd.range().empty());
}

TEST(HistoryCommandParser, SaveWithoutFilepath) {
    HistoryCommandParser parser;
    CommandTokenStream   ts(":history save");
    auto r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    EXPECT_EQ(static_cast<const HistoryCommand&>(*r->get()).action(),
              HistoryCommand::Action::Save);
}

// ============================================================
// Status filter flags
// ============================================================

TEST(HistoryCommandParser, FlagErrors) {
    HistoryCommandParser parser;
    CommandTokenStream   ts(":history --errors");
    auto r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    auto& cmd = static_cast<const HistoryCommand&>(*r->get());
    EXPECT_TRUE(cmd.has_flag(HistoryCommand::Flag::Errors));
}

TEST(HistoryCommandParser, FlagSuccess) {
    HistoryCommandParser parser;
    CommandTokenStream   ts(":history --success");
    auto r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    EXPECT_TRUE(static_cast<const HistoryCommand&>(*r->get())
                    .has_flag(HistoryCommand::Flag::Success));
}

TEST(HistoryCommandParser, FlagWarning) {
    HistoryCommandParser parser;
    CommandTokenStream   ts(":history --warning");
    auto r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    EXPECT_TRUE(static_cast<const HistoryCommand&>(*r->get())
                    .has_flag(HistoryCommand::Flag::Warning));
}

TEST(HistoryCommandParser, FlagInfo) {
    HistoryCommandParser parser;
    CommandTokenStream   ts(":history --info");
    auto r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    EXPECT_TRUE(static_cast<const HistoryCommand&>(*r->get())
                    .has_flag(HistoryCommand::Flag::Info));
}

TEST(HistoryCommandParser, MultipleFlags) {
    HistoryCommandParser parser;
    CommandTokenStream   ts(":history --errors --success");
    auto r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    auto& cmd = static_cast<const HistoryCommand&>(*r->get());
    EXPECT_TRUE(cmd.has_flag(HistoryCommand::Flag::Errors));
    EXPECT_TRUE(cmd.has_flag(HistoryCommand::Flag::Success));
}

TEST(HistoryCommandParser, NoFlagsMeansNoFilter) {
    HistoryCommandParser parser;
    CommandTokenStream   ts(":history");
    auto r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    EXPECT_FALSE(static_cast<const HistoryCommand&>(*r->get()).has_any_flag());
}

// ============================================================
// is_readonly
// ============================================================

TEST(HistoryCommandParser, ShowIsReadonly) {
    HistoryCommandParser parser;
    CommandTokenStream   ts(":history");
    auto r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    EXPECT_TRUE(static_cast<const HistoryCommand&>(*r->get()).is_readonly());
}

TEST(HistoryCommandParser, SearchIsReadonly) {
    HistoryCommandParser parser;
    CommandTokenStream   ts(":history search x");
    auto r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    EXPECT_TRUE(static_cast<const HistoryCommand&>(*r->get()).is_readonly());
}

TEST(HistoryCommandParser, ClearIsNotReadonly) {
    HistoryCommandParser parser;
    CommandTokenStream   ts(":history clear");
    auto r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    EXPECT_FALSE(static_cast<const HistoryCommand&>(*r->get()).is_readonly());
}

TEST(HistoryCommandParser, SaveIsNotReadonly) {
    HistoryCommandParser parser;
    CommandTokenStream   ts(":history save out.txt");
    auto r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    EXPECT_FALSE(static_cast<const HistoryCommand&>(*r->get()).is_readonly());
}
