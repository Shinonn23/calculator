#include <gtest/gtest.h>

#include "core/span.hpp"

using namespace math_solver;

TEST(SpanTest, DefaultConstruction) {
    Span s;
    EXPECT_EQ(s.start, 0u);
    EXPECT_EQ(s.end, 0u);
}

TEST(SpanTest, ParameterizedConstruction) {
    Span s(3, 7);
    EXPECT_EQ(s.start, 3u);
    EXPECT_EQ(s.end, 7u);
}

TEST(SpanTest, Length) {
    EXPECT_EQ(Span(2, 9).length(), 7u);
    EXPECT_EQ(Span(0, 0).length(), 0u);
    EXPECT_EQ(Span(5, 5).length(), 0u);
}

TEST(SpanTest, EmptyWhenStartEqualsEnd) {
    EXPECT_TRUE(Span(4, 4).empty());
    EXPECT_TRUE(Span(0, 0).empty());
}

TEST(SpanTest, NotEmptyWhenStartLessThanEnd) {
    EXPECT_FALSE(Span(1, 5).empty());
    EXPECT_FALSE(Span(0, 1).empty());
}

TEST(SpanTest, MergeDisjoint) {
    Span m = Span(1, 4).merge(Span(6, 10));
    EXPECT_EQ(m.start, 1u);
    EXPECT_EQ(m.end, 10u);
}

TEST(SpanTest, MergeOverlapping) {
    Span m = Span(2, 8).merge(Span(5, 12));
    EXPECT_EQ(m.start, 2u);
    EXPECT_EQ(m.end, 12u);
}

TEST(SpanTest, MergeSelf) {
    Span a(3, 7);
    Span m = a.merge(a);
    EXPECT_EQ(m.start, a.start);
    EXPECT_EQ(m.end, a.end);
}

TEST(SpanTest, MergeAdjacent) {
    Span m = Span(0, 3).merge(Span(3, 6));
    EXPECT_EQ(m.start, 0u);
    EXPECT_EQ(m.end, 6u);
}

TEST(SpanTest, MergeIsCommutative) {
    Span a(1, 5), b(3, 9);
    Span ab = a.merge(b);
    Span ba = b.merge(a);
    EXPECT_EQ(ab.start, ba.start);
    EXPECT_EQ(ab.end, ba.end);
}
