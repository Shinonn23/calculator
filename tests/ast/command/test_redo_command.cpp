#include <gtest/gtest.h>

#include "ast/command/redo_command.hpp"
#include "ast/mock_visitors.hpp"
#include "diagnostics/sink.hpp"

using namespace math_solver;
using test_helpers::MockCommandVisitor;

TEST(RedoCommandTest, RangeEmptyByDefault) {
    EXPECT_TRUE(RedoCommand("redo").range().empty());
}

TEST(RedoCommandTest, SetRange) {
    RedoCommand c("redo 2-4");
    c.set_range({2, 3, 4});
    ASSERT_EQ(c.range().size(), 3u);
    EXPECT_EQ(c.range()[0], 2);
    EXPECT_EQ(c.range()[2], 4);
}

TEST(RedoCommandTest, SetRangeSingleEntry) {
    RedoCommand c("redo 5");
    c.set_range({5});
    ASSERT_EQ(c.range().size(), 1u);
    EXPECT_EQ(c.range()[0], 5);
}

TEST(RedoCommandTest, RawCommandStored) {
    EXPECT_EQ(RedoCommand("redo").raw_command(), "redo");
}

TEST(RedoCommandTest, AcceptDispatchesRedoCommand) {
    RedoCommand        c("redo");
    MockCommandVisitor v;
    DiagnosticSink     sink;
    c.accept(v, sink);
    EXPECT_EQ(v.last, MockCommandVisitor::Visited::Redo);
}
