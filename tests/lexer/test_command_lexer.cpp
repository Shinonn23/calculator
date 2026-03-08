#include <gtest/gtest.h>

#include "lexer/command/command_lexer.hpp"

using namespace math_solver;

// ============================================================
// Helpers
// ============================================================

static CommandToken next(CommandLexer& l) { return l.next_token(); }

// ============================================================
// End-of-input (Eof)
// ============================================================

TEST(CommandLexer, EmptyInputIsEof) {
    CommandLexer l("");
    EXPECT_EQ(next(l).type, CommandTokenType::Eof);
}

TEST(CommandLexer, WhitespaceOnlyIsEof) {
    CommandLexer l("   ");
    EXPECT_EQ(next(l).type, CommandTokenType::Eof);
}

TEST(CommandLexer, EofValueIsEmpty) {
    CommandLexer l("");
    EXPECT_EQ(next(l).value, "");
}

// ============================================================
// Command tokens  (:set, :solve, …)
// ============================================================

TEST(CommandLexer, SimpleCommand) {
    CommandLexer l(":set");
    auto         t = next(l);
    EXPECT_EQ(t.type, CommandTokenType::Command);
    EXPECT_EQ(t.value, ":set");
}

TEST(CommandLexer, SolveCommand) {
    CommandLexer l(":solve");
    auto         t = next(l);
    EXPECT_EQ(t.type, CommandTokenType::Command);
    EXPECT_EQ(t.value, ":solve");
}

TEST(CommandLexer, HistoryCommand) {
    CommandLexer l(":history");
    EXPECT_EQ(next(l).value, ":history");
}

TEST(CommandLexer, CommandFollowedByWord) {
    CommandLexer l(":set x");
    EXPECT_EQ(next(l).type, CommandTokenType::Command);
    auto w = next(l);
    EXPECT_EQ(w.type, CommandTokenType::Word);
    EXPECT_EQ(w.value, "x");
}

TEST(CommandLexer, CommandFollowedByEof) {
    CommandLexer l(":solve");
    next(l);
    EXPECT_EQ(next(l).type, CommandTokenType::Eof);
}

// ============================================================
// Flag tokens  (-flag, --flag, --key=value)
// ============================================================

TEST(CommandLexer, SingleDashFlag) {
    CommandLexer l("-isolated");
    auto         t = next(l);
    EXPECT_EQ(t.type, CommandTokenType::Flag);
    EXPECT_EQ(t.value, "-isolated");
}

TEST(CommandLexer, DoubleDashFlag) {
    CommandLexer l("--no-save");
    auto         t = next(l);
    EXPECT_EQ(t.type, CommandTokenType::Flag);
    EXPECT_EQ(t.value, "--no-save");
}

TEST(CommandLexer, DoubleDashFlagWithEquals) {
    CommandLexer l("--method=lu");
    auto         t = next(l);
    EXPECT_EQ(t.type, CommandTokenType::Flag);
    EXPECT_EQ(t.value, "--method=lu");
}

TEST(CommandLexer, DoubleDashFlagWithEqualsGauss) {
    CommandLexer l("--method=gauss");
    auto         t = next(l);
    EXPECT_EQ(t.type, CommandTokenType::Flag);
    EXPECT_EQ(t.value, "--method=gauss");
}

TEST(CommandLexer, MultipleFlags) {
    CommandLexer l("--no-save --fraction");
    EXPECT_EQ(next(l).type, CommandTokenType::Flag);
    EXPECT_EQ(next(l).type, CommandTokenType::Flag);
    EXPECT_EQ(next(l).type, CommandTokenType::Eof);
}

// Negative numbers must NOT become flags
TEST(CommandLexer, NegativeNumberIsWord) {
    CommandLexer l("-3");
    auto         t = next(l);
    EXPECT_EQ(t.type, CommandTokenType::Word);
    EXPECT_EQ(t.value, "-3");
}

TEST(CommandLexer, NegativeFloatIsWord) {
    CommandLexer l("-3.14");
    auto         t = next(l);
    EXPECT_EQ(t.type, CommandTokenType::Word);
    EXPECT_EQ(t.value, "-3.14");
}

// Lone dash is NOT a flag (no alpha or '-' after it)
TEST(CommandLexer, LoneDashIsWord) {
    CommandLexer l("-");
    auto         t = next(l);
    EXPECT_EQ(t.type, CommandTokenType::Word);
}

// ============================================================
// Word tokens
// ============================================================

TEST(CommandLexer, SimpleWord) {
    CommandLexer l("hello");
    auto         t = next(l);
    EXPECT_EQ(t.type, CommandTokenType::Word);
    EXPECT_EQ(t.value, "hello");
}

TEST(CommandLexer, WordWithDigits) {
    CommandLexer l("x1");
    auto         t = next(l);
    EXPECT_EQ(t.type, CommandTokenType::Word);
    EXPECT_EQ(t.value, "x1");
}

TEST(CommandLexer, WordWithEquals) {
    // An unquoted word containing '=' that doesn't start with '-' is a Word
    CommandLexer l("key=val");
    auto         t = next(l);
    EXPECT_EQ(t.type, CommandTokenType::Word);
    EXPECT_EQ(t.value, "key=val");
}

// A standalone comma is lexed as a Word (CommandLexer has no special comma rule)
TEST(CommandLexer, StandaloneCommaIsWord) {
    CommandLexer l(",");
    auto         t = next(l);
    EXPECT_EQ(t.type, CommandTokenType::Word);
    EXPECT_EQ(t.value, ",");
}

// ============================================================
// Quoted string tokens
// ============================================================

TEST(CommandLexer, QuotedStringStripsQuotes) {
    CommandLexer l("\"hello world\"");
    auto         t = next(l);
    EXPECT_EQ(t.type, CommandTokenType::QuotedString);
    EXPECT_EQ(t.value, "hello world");
}

TEST(CommandLexer, QuotedStringWithSpaces) {
    CommandLexer l("\"x^2 + 3*x - 1\"");
    auto         t = next(l);
    EXPECT_EQ(t.type, CommandTokenType::QuotedString);
    EXPECT_EQ(t.value, "x^2 + 3*x - 1");
}

TEST(CommandLexer, EmptyQuotedString) {
    CommandLexer l("\"\"");
    auto         t = next(l);
    EXPECT_EQ(t.type, CommandTokenType::QuotedString);
    EXPECT_EQ(t.value, "");
}

TEST(CommandLexer, QuotedStringWithEquation) {
    CommandLexer l("\"x + 1 = 0\"");
    auto         t = next(l);
    EXPECT_EQ(t.type, CommandTokenType::QuotedString);
    EXPECT_EQ(t.value, "x + 1 = 0");
}

// ============================================================
// Multi-token sequences
// ============================================================

TEST(CommandLexer, SetSequence) {
    CommandLexer l(":set x 3+2");
    EXPECT_EQ(next(l).type, CommandTokenType::Command);
    EXPECT_EQ(next(l).value, "x");
    EXPECT_EQ(next(l).value, "3+2");
    EXPECT_EQ(next(l).type, CommandTokenType::Eof);
}

TEST(CommandLexer, SolveWithFlagAndQuotedExpr) {
    CommandLexer l(":solve --no-save \"x + 1 = 0\"");
    EXPECT_EQ(next(l).type, CommandTokenType::Command);
    auto flag = next(l);
    EXPECT_EQ(flag.type, CommandTokenType::Flag);
    EXPECT_EQ(flag.value, "--no-save");
    auto expr = next(l);
    EXPECT_EQ(expr.type, CommandTokenType::QuotedString);
    EXPECT_EQ(expr.value, "x + 1 = 0");
    EXPECT_EQ(next(l).type, CommandTokenType::Eof);
}

TEST(CommandLexer, LoadWithFlags) {
    CommandLexer l(":load --dry-run script.msl");
    EXPECT_EQ(next(l).type, CommandTokenType::Command);
    EXPECT_EQ(next(l).type, CommandTokenType::Flag);
    EXPECT_EQ(next(l).value, "script.msl");
    EXPECT_EQ(next(l).type, CommandTokenType::Eof);
}

// ============================================================
// Span tracking
// ============================================================

TEST(CommandLexer, CommandSpanIsCorrect) {
    CommandLexer l(":set");
    auto         t = next(l);
    EXPECT_EQ(t.start, 0u);
    EXPECT_EQ(t.end,   4u);
}

TEST(CommandLexer, WordSpanSkipsLeadingWhitespace) {
    CommandLexer l("  abc");
    auto         t = next(l);
    EXPECT_EQ(t.start, 2u);
    EXPECT_EQ(t.end,   5u);
}

TEST(CommandLexer, SecondTokenSpan) {
    CommandLexer l(":set xy");
    next(l);           // :set spans 0–4
    auto w = next(l);  // xy spans 5–7
    EXPECT_EQ(w.start, 5u);
    EXPECT_EQ(w.end,   7u);
}

// ============================================================
// consume_rest
// ============================================================

TEST(CommandLexer, ConsumeRestReturnsRemainder) {
    CommandLexer l(":solve x + 1 = 0");
    next(l); // :solve
    EXPECT_EQ(l.consume_rest(), "x + 1 = 0");
}

TEST(CommandLexer, ConsumeRestTrimsTrailingWhitespace) {
    CommandLexer l(":solve   x + 1  ");
    next(l);
    EXPECT_EQ(l.consume_rest(), "x + 1");
}

TEST(CommandLexer, ConsumeRestOnEmptyIsEmpty) {
    CommandLexer l(":solve");
    next(l);
    EXPECT_EQ(l.consume_rest(), "");
}

TEST(CommandLexer, ConsumeRestPreservesInternalWhitespace) {
    CommandLexer l(":set x^2 + 1");
    next(l);
    EXPECT_EQ(l.consume_rest(), "x^2 + 1");
}
