// ============================================================
// Unit tests สำหรับ utility functions ใน utils.h
// ครอบคลุม: trim, split, starts_with, ends_with
// ============================================================

#include "../src/common/utils.h"
#include <gtest/gtest.h>

using namespace math_solver;
using namespace std;

// ============================================================
// trim tests
// ============================================================

// ทดสอบ trim กับ string ปกติ (ไม่มี whitespace ข้างหน้า/หลัง)
TEST(TrimTest, NoWhitespace) { EXPECT_EQ(trim("hello"), "hello"); }

// ทดสอบ trim กับ leading spaces
TEST(TrimTest, LeadingSpaces) { EXPECT_EQ(trim("   hello"), "hello"); }

// ทดสอบ trim กับ trailing spaces
TEST(TrimTest, TrailingSpaces) { EXPECT_EQ(trim("hello   "), "hello"); }

// ทดสอบ trim กับ leading + trailing spaces
TEST(TrimTest, BothSidesSpaces) {
    EXPECT_EQ(trim("   hello world   "), "hello world");
}

// ทดสอบ trim กับ string ที่มีแต่ whitespace (ต้องเป็น empty)
TEST(TrimTest, OnlyWhitespace) { EXPECT_EQ(trim("     "), ""); }

// ทดสอบ trim กับ empty string
TEST(TrimTest, EmptyString) { EXPECT_EQ(trim(""), ""); }

// ทดสอบ trim กับ tabs และ newlines
TEST(TrimTest, TabsAndNewlines) { EXPECT_EQ(trim("\t\nhello\r\n"), "hello"); }

// ============================================================
// split tests
// ============================================================

// ทดสอบ split กับ string ปกติ (แยกด้วย space)
TEST(SplitTest, BasicSplit) {
    vector<string> result = split("hello world");
    ASSERT_EQ(result.size(), 2u);
    EXPECT_EQ(result[0], "hello");
    EXPECT_EQ(result[1], "world");
}

// ทดสอบ split กับ multiple spaces ระหว่างคำ
TEST(SplitTest, MultipleSpaces) {
    vector<string> result = split("a   b   c");
    ASSERT_EQ(result.size(), 3u);
    EXPECT_EQ(result[0], "a");
    EXPECT_EQ(result[1], "b");
    EXPECT_EQ(result[2], "c");
}

// ทดสอบ split กับ empty string (ต้องได้ vector ว่าง)
TEST(SplitTest, EmptyString) {
    vector<string> result = split("");
    EXPECT_TRUE(result.empty());
}

// ทดสอบ split กับ whitespace only (ต้องได้ vector ว่าง)
TEST(SplitTest, WhitespaceOnly) {
    vector<string> result = split("   ");
    EXPECT_TRUE(result.empty());
}

// ทดสอบ split กับคำเดียว (ไม่มี space)
TEST(SplitTest, SingleWord) {
    vector<string> result = split("hello");
    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0], "hello");
}

// ทดสอบ split กับ leading/trailing spaces
TEST(SplitTest, LeadingTrailingSpaces) {
    vector<string> result = split("  hello world  ");
    ASSERT_EQ(result.size(), 2u);
    EXPECT_EQ(result[0], "hello");
    EXPECT_EQ(result[1], "world");
}

// ============================================================
// starts_with tests
// ============================================================

// ทดสอบ starts_with กับ prefix ที่ตรง
TEST(StartsWithTest, MatchingPrefix) {
    EXPECT_TRUE(starts_with("hello world", "hello"));
}

// ทดสอบ starts_with กับ prefix ที่ไม่ตรง
TEST(StartsWithTest, NonMatchingPrefix) {
    EXPECT_FALSE(starts_with("hello world", "world"));
}

// ทดสอบ starts_with กับ prefix ที่ยาวกว่า string
TEST(StartsWithTest, PrefixLongerThanString) {
    EXPECT_FALSE(starts_with("hi", "hello"));
}

// ทดสอบ starts_with กับ empty prefix (ต้องเป็น true เสมอ)
TEST(StartsWithTest, EmptyPrefix) { EXPECT_TRUE(starts_with("hello", "")); }

// ทดสอบ starts_with กับ empty string
TEST(StartsWithTest, EmptyString) { EXPECT_FALSE(starts_with("", "hello")); }

// ทดสอบ starts_with กับทั้งสอง empty
TEST(StartsWithTest, BothEmpty) { EXPECT_TRUE(starts_with("", "")); }

// ทดสอบ starts_with กับ string เท่ากับ prefix
TEST(StartsWithTest, ExactMatch) { EXPECT_TRUE(starts_with("hello", "hello")); }

// ============================================================
// ends_with tests
// ============================================================

// ทดสอบ ends_with กับ suffix ที่ตรง
TEST(EndsWithTest, MatchingSuffix) {
    EXPECT_TRUE(ends_with("hello world", "world"));
}

// ทดสอบ ends_with กับ suffix ที่ไม่ตรง
TEST(EndsWithTest, NonMatchingSuffix) {
    EXPECT_FALSE(ends_with("hello world", "hello"));
}

// ทดสอบ ends_with กับ suffix ที่ยาวกว่า string
TEST(EndsWithTest, SuffixLongerThanString) {
    EXPECT_FALSE(ends_with("hi", "hello"));
}

// ทดสอบ ends_with กับ empty suffix (ต้องเป็น true เสมอ)
TEST(EndsWithTest, EmptySuffix) { EXPECT_TRUE(ends_with("hello", "")); }

// ทดสอบ ends_with กับ empty string
TEST(EndsWithTest, EmptyString) { EXPECT_FALSE(ends_with("", "hello")); }

// ทดสอบ ends_with กับทั้งสอง empty
TEST(EndsWithTest, BothEmpty) { EXPECT_TRUE(ends_with("", "")); }

// ทดสอบ ends_with กับ string เท่ากับ suffix
TEST(EndsWithTest, ExactMatch) { EXPECT_TRUE(ends_with("hello", "hello")); }
