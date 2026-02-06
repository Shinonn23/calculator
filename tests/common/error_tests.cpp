// ============================================================
// Unit tests สำหรับ Error classes ทั้งหมด
// ครอบคลุม: MathError, ParseError, UndefinedVariableError,
//           NonLinearError, MultipleUnknownsError,
//           NoSolutionError, InfiniteSolutionsError,
//           InvalidEquationError, ReservedKeywordError
// ============================================================

#include "common/error.h"
#include <gtest/gtest.h>
#include <string>

using namespace math_solver;
using namespace std;

// ============================================================
// MathError (base class)
// ============================================================

// ทดสอบ MathError message
TEST(MathErrorTest, Message) {
    MathError err("test error");
    EXPECT_STREQ(err.what(), "test error");
}

// ทดสอบ MathError กับ Span
TEST(MathErrorTest, WithSpan) {
    MathError err("error at pos", Span(3, 5));
    EXPECT_EQ(err.span().start, 3u);
    EXPECT_EQ(err.span().end, 5u);
}

// ทดสอบ MathError กับ input string
TEST(MathErrorTest, WithInput) {
    MathError err("bad", Span(0, 3), "hello");
    EXPECT_EQ(err.input(), "hello");
}

// ทดสอบ set_input
TEST(MathErrorTest, SetInput) {
    MathError err("x");
    err.set_input("new input");
    EXPECT_EQ(err.input(), "new input");
}

// ทดสอบ format() เมื่อไม่มี input (fallback)
TEST(MathErrorTest, FormatNoInput) {
    MathError err("oops");
    string    formatted = err.format();
    EXPECT_NE(formatted.find("oops"), string::npos);
}

// ทดสอบ format() เมื่อมี input
TEST(MathErrorTest, FormatWithInput) {
    MathError err("bad token", Span(0, 3), "1+2");
    string    formatted = err.format();
    // ต้องมีทั้ง error message และ input context
    EXPECT_NE(formatted.find("bad token"), string::npos);
}

// ============================================================
// ParseError
// ============================================================

// ทดสอบ ParseError เป็น subclass ของ MathError
TEST(ParseErrorTest, InheritsFromMathError) {
    ParseError err("unexpected token", Span(5, 6), "1 + ) 2");
    // catch ด้วย MathError ได้
    try {
        throw err;
    } catch (const MathError& e) {
        EXPECT_STREQ(e.what(), "unexpected token");
    }
}

// ทดสอบ ParseError message
TEST(ParseErrorTest, Message) {
    ParseError err("expected ')'");
    EXPECT_STREQ(err.what(), "expected ')'");
}

// ============================================================
// UndefinedVariableError
// ============================================================

// ทดสอบ message format
TEST(UndefinedVariableErrorTest, Message) {
    UndefinedVariableError err("x");
    EXPECT_STREQ(err.what(), "undefined variable 'x'");
}

// ทดสอบ var_name() accessor
TEST(UndefinedVariableErrorTest, VarName) {
    UndefinedVariableError err("myVar", Span(0, 5));
    EXPECT_EQ(err.var_name(), "myVar");
}

// ทดสอบ inheritance จาก MathError
TEST(UndefinedVariableErrorTest, InheritsFromMathError) {
    UndefinedVariableError err("z");
    const MathError*       base = &err;
    EXPECT_STREQ(base->what(), "undefined variable 'z'");
}

// ============================================================
// NonLinearError
// ============================================================

// ทดสอบ message
TEST(NonLinearErrorTest, Message) {
    NonLinearError err("non-linear term: x*y");
    EXPECT_STREQ(err.what(), "non-linear term: x*y");
}

// ทดสอบ span
TEST(NonLinearErrorTest, WithSpan) {
    NonLinearError err("bad", Span(2, 5), "x * y");
    EXPECT_EQ(err.span().start, 2u);
    EXPECT_EQ(err.span().end, 5u);
}

// ============================================================
// MultipleUnknownsError
// ============================================================

// ทดสอบ message format — ต้องรวมชื่อตัวแปรทั้งหมด
TEST(MultipleUnknownsErrorTest, Message) {
    vector<string>        vars = {"x", "y"};
    MultipleUnknownsError err(vars);
    string                msg = err.what();
    EXPECT_NE(msg.find("x"), string::npos);
    EXPECT_NE(msg.find("y"), string::npos);
    EXPECT_NE(msg.find("multiple unknowns"), string::npos);
}

// ทดสอบ unknowns() accessor
TEST(MultipleUnknownsErrorTest, UnknownsAccessor) {
    vector<string>        vars = {"a", "b", "c"};
    MultipleUnknownsError err(vars);
    EXPECT_EQ(err.unknowns().size(), 3u);
    EXPECT_EQ(err.unknowns()[0], "a");
    EXPECT_EQ(err.unknowns()[1], "b");
    EXPECT_EQ(err.unknowns()[2], "c");
}

// ทดสอบ single unknown
TEST(MultipleUnknownsErrorTest, SingleUnknown) {
    vector<string>        vars = {"x"};
    MultipleUnknownsError err(vars);
    string                msg = err.what();
    EXPECT_NE(msg.find("x"), string::npos);
}

// ============================================================
// NoSolutionError
// ============================================================

// ทดสอบ default message
TEST(NoSolutionErrorTest, DefaultMessage) {
    NoSolutionError err;
    EXPECT_STREQ(err.what(), "equation has no solution");
}

// ทดสอบ custom message
TEST(NoSolutionErrorTest, CustomMessage) {
    NoSolutionError err("3 != 0");
    EXPECT_STREQ(err.what(), "3 != 0");
}

// ============================================================
// InfiniteSolutionsError
// ============================================================

// ทดสอบ default message
TEST(InfiniteSolutionsErrorTest, DefaultMessage) {
    InfiniteSolutionsError err;
    EXPECT_STREQ(err.what(), "equation has infinite solutions");
}

// ทดสอบ custom message
TEST(InfiniteSolutionsErrorTest, CustomMessage) {
    InfiniteSolutionsError err("0 = 0 always true");
    EXPECT_STREQ(err.what(), "0 = 0 always true");
}

// ============================================================
// InvalidEquationError
// ============================================================

// ทดสอบ message
TEST(InvalidEquationErrorTest, Message) {
    InvalidEquationError err("variable 'x' not found in equation");
    EXPECT_STREQ(err.what(), "variable 'x' not found in equation");
}

// ทดสอบ span
TEST(InvalidEquationErrorTest, WithSpan) {
    InvalidEquationError err("bad", Span(0, 10), "input");
    EXPECT_EQ(err.span().start, 0u);
    EXPECT_EQ(err.span().end, 10u);
}

// ============================================================
// ReservedKeywordError
// ============================================================

// ทดสอบ message format
TEST(ReservedKeywordErrorTest, Message) {
    ReservedKeywordError err("solve");
    string               msg = err.what();
    EXPECT_NE(msg.find("solve"), string::npos);
    EXPECT_NE(msg.find("reserved keyword"), string::npos);
}

// ทดสอบ span
TEST(ReservedKeywordErrorTest, WithSpan) {
    ReservedKeywordError err("set", Span(0, 3), "set x 5");
    EXPECT_EQ(err.span().start, 0u);
    EXPECT_EQ(err.span().end, 3u);
}

// ============================================================
// Inheritance chain tests
// ============================================================

// ทดสอบว่า error classes ทั้งหมด catch ด้วย std::exception ได้
TEST(ErrorInheritanceTest, AllCatchableAsException) {
    // ParseError
    EXPECT_THROW(throw ParseError("p"), std::exception);
    // UndefinedVariableError
    EXPECT_THROW(throw UndefinedVariableError("v"), std::exception);
    // NonLinearError
    EXPECT_THROW(throw NonLinearError("nl"), std::exception);
    // NoSolutionError
    EXPECT_THROW(throw NoSolutionError(), std::exception);
    // InfiniteSolutionsError
    EXPECT_THROW(throw InfiniteSolutionsError(), std::exception);
    // InvalidEquationError
    EXPECT_THROW(throw InvalidEquationError("ie"), std::exception);
    // ReservedKeywordError
    EXPECT_THROW(throw ReservedKeywordError("rk"), std::exception);
}

// ทดสอบว่า subclass ทุกตัว catch ด้วย MathError ได้
TEST(ErrorInheritanceTest, AllCatchableAsMathError) {
    EXPECT_THROW(throw ParseError("p"), MathError);
    EXPECT_THROW(throw UndefinedVariableError("v"), MathError);
    EXPECT_THROW(throw NonLinearError("nl"), MathError);
    EXPECT_THROW(throw NoSolutionError(), MathError);
    EXPECT_THROW(throw InfiniteSolutionsError(), MathError);
    EXPECT_THROW(throw InvalidEquationError("ie"), MathError);
    EXPECT_THROW(throw ReservedKeywordError("rk"), MathError);
}
