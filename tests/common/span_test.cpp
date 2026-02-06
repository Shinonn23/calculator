// ============================================================
// Unit tests สำหรับ Span struct และ error/warning formatting
// ครอบคลุม: Span constructor, merge, length, empty,
//           format_error_at_span, format_warning_at_span
// ============================================================

#include "../src/common/span.h"
#include <gtest/gtest.h>

using namespace math_solver;
using namespace std;

// ============================================================
// Span struct tests
// ============================================================

// ทดสอบ default constructor (start=0, end=0)
TEST(SpanTest, DefaultConstructor) {
    Span span;
    EXPECT_EQ(span.start, 0u);
    EXPECT_EQ(span.end, 0u);
}

// ทดสอบ parameterized constructor
TEST(SpanTest, ParameterizedConstructor) {
    Span span(3, 7);
    EXPECT_EQ(span.start, 3u);
    EXPECT_EQ(span.end, 7u);
}

// ทดสอบ length() - ความยาวของ span
TEST(SpanTest, Length) {
    Span span(2, 5);
    EXPECT_EQ(span.length(), 3u);
}

// ทดสอบ length() เมื่อ start == end (ต้องเป็น 0)
TEST(SpanTest, ZeroLength) {
    Span span(5, 5);
    EXPECT_EQ(span.length(), 0u);
}

// ทดสอบ empty() - true เมื่อ start == end
TEST(SpanTest, EmptySpan) {
    Span span(3, 3);
    EXPECT_TRUE(span.empty());
}

// ทดสอบ empty() - false เมื่อ start != end
TEST(SpanTest, NonEmptySpan) {
    Span span(1, 4);
    EXPECT_FALSE(span.empty());
}

// ทดสอบ merge - รวม 2 span ให้ครอบคลุมทั้งสอง
TEST(SpanTest, MergeOverlapping) {
    Span a(2, 5);
    Span b(3, 7);
    Span merged = a.merge(b);
    // ต้อง merge เป็น (min(2,3), max(5,7)) = (2, 7)
    EXPECT_EQ(merged.start, 2u);
    EXPECT_EQ(merged.end, 7u);
}

// ทดสอบ merge กับ span ที่ไม่ overlap (disjoint)
TEST(SpanTest, MergeDisjoint) {
    Span a(1, 3);
    Span b(5, 8);
    Span merged = a.merge(b);
    // ต้อง merge เป็น (1, 8)
    EXPECT_EQ(merged.start, 1u);
    EXPECT_EQ(merged.end, 8u);
}

// ทดสอบ merge เมื่อ span แรกครอบ span ที่สอง
TEST(SpanTest, MergeContained) {
    Span a(1, 10);
    Span b(3, 5);
    Span merged = a.merge(b);
    EXPECT_EQ(merged.start, 1u);
    EXPECT_EQ(merged.end, 10u);
}

// ทดสอบ merge กลับด้าน (b.merge(a) ต้องได้ผลเหมือนกัน)
TEST(SpanTest, MergeCommutative) {
    Span a(2, 5);
    Span b(3, 7);
    Span ab = a.merge(b);
    Span ba = b.merge(a);
    EXPECT_EQ(ab.start, ba.start);
    EXPECT_EQ(ab.end, ba.end);
}

// ============================================================
// format_error_at_span tests
// ============================================================

// ทดสอบ format error กับ span ที่ชี้ไปยังตำแหน่งกลาง string
TEST(FormatErrorTest, BasicError) {
    string result =
        format_error_at_span("unexpected token", "2 + * 3", Span(4, 5));
    // ต้องมี "Error: unexpected token"
    EXPECT_TRUE(result.find("Error: unexpected token") != string::npos);
    // ต้องมี source line
    EXPECT_TRUE(result.find("2 + * 3") != string::npos);
    // ต้องมี caret (^) ชี้ไปที่ตำแหน่ง 4
    EXPECT_TRUE(result.find("^") != string::npos);
}

// ทดสอบ format error เมื่อ span อยู่ต้น string (start=0)
TEST(FormatErrorTest, ErrorAtStart) {
    string result = format_error_at_span("bad start", "xyz", Span(0, 1));
    EXPECT_TRUE(result.find("Error: bad start") != string::npos);
    EXPECT_TRUE(result.find("xyz") != string::npos);
}

// ทดสอบ format error เมื่อ span ครอบหลายตัวอักษร (ต้องมี ^ หลายตัว)
TEST(FormatErrorTest, MultiCharSpan) {
    string result = format_error_at_span("bad token", "abcdef", Span(1, 4));
    // ต้องมี "^^^" (3 carets สำหรับ length=3)
    EXPECT_TRUE(result.find("^^^") != string::npos);
}

// ทดสอบ format error เมื่อ span length = 0 (ต้องแสดง ^ อย่างน้อย 1 ตัว)
TEST(FormatErrorTest, ZeroLengthSpan) {
    string result = format_error_at_span("unexpected", "abc", Span(1, 1));
    EXPECT_TRUE(result.find("^") != string::npos);
}

// ทดสอบ format error เมื่อ input เป็น empty string
TEST(FormatErrorTest, EmptyInput) {
    string result = format_error_at_span("empty", "", Span(0, 0));
    EXPECT_TRUE(result.find("Error: empty") != string::npos);
}

// ============================================================
// format_warning_at_span tests
// ============================================================

// ทดสอบ format warning - ต้องใช้ ~ แทน ^
TEST(FormatWarningTest, BasicWarning) {
    string result =
        format_warning_at_span("shadowed variable", "x + y = 5", Span(0, 1));
    EXPECT_TRUE(result.find("Warning: shadowed variable") != string::npos);
    EXPECT_TRUE(result.find("x + y = 5") != string::npos);
    EXPECT_TRUE(result.find("~") != string::npos);
}

// ทดสอบ format warning กับ multi-char span (ต้องมี ~ หลายตัว)
TEST(FormatWarningTest, MultiCharSpanWarning) {
    string result =
        format_warning_at_span("long warning", "abcdef", Span(2, 5));
    // ต้องมี "~~~" (3 tildes)
    EXPECT_TRUE(result.find("~~~") != string::npos);
}

// ทดสอบ format warning เมื่อ span length = 0 (ต้องแสดง ~ อย่างน้อย 1 ตัว)
TEST(FormatWarningTest, ZeroLengthSpanWarning) {
    string result = format_warning_at_span("warn", "abc", Span(1, 1));
    EXPECT_TRUE(result.find("~") != string::npos);
}
