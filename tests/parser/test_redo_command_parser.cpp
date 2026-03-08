#include <gtest/gtest.h>

#include "ast/command/redo_command.hpp"
#include "lexer/command/command_token_stream.hpp"
#include "parser/command/subparsers/redo_command_parser.hpp"

using namespace math_solver;

// ============================================================
// No argument → redo last command (empty range)
// ============================================================

TEST(RedoCommandParser, BareRedoEmptyRange) {
    RedoCommandParser  parser;
    CommandTokenStream ts(":redo");
    auto r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    EXPECT_TRUE(static_cast<const RedoCommand&>(*r->get()).range().empty());
}

// ============================================================
// Numeric selector (single index)
// ============================================================

TEST(RedoCommandParser, SingleIndex) {
    RedoCommandParser  parser;
    CommandTokenStream ts(":redo 3");
    auto r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    auto& cmd = static_cast<const RedoCommand&>(*r->get());
    EXPECT_FALSE(cmd.range().empty());
    EXPECT_EQ(cmd.range()[0], 3);
}

TEST(RedoCommandParser, SingleIndexOne) {
    RedoCommandParser  parser;
    CommandTokenStream ts(":redo 1");
    auto r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    auto& cmd = static_cast<const RedoCommand&>(*r->get());
    ASSERT_FALSE(cmd.range().empty());
    EXPECT_EQ(cmd.range()[0], 1);
}

// ============================================================
// Range selector
// ============================================================

TEST(RedoCommandParser, RangeSelector) {
    RedoCommandParser  parser;
    CommandTokenStream ts(":redo 1-3");
    auto r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    auto& cmd = static_cast<const RedoCommand&>(*r->get());
    EXPECT_EQ(cmd.range().size(), 3u);
    EXPECT_EQ(cmd.range()[0], 1);
    EXPECT_EQ(cmd.range()[2], 3);
}

// ============================================================
// Non-numeric token → ignored (treated as bare redo)
// ============================================================

TEST(RedoCommandParser, NonNumericArgIsIgnored) {
    RedoCommandParser  parser;
    CommandTokenStream ts(":redo last");
    auto r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    // Non-numeric selector is not consumed; range remains empty
    EXPECT_TRUE(static_cast<const RedoCommand&>(*r->get()).range().empty());
}

// ============================================================
// Raw input preserved
// ============================================================

TEST(RedoCommandParser, RawInputPreserved) {
    RedoCommandParser  parser;
    CommandTokenStream ts(":redo 2");
    auto r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    EXPECT_EQ(r->get()->raw_command(), ":redo 2");
}
