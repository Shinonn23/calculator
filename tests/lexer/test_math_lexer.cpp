#include <gtest/gtest.h>

#include "lexer/math/math_lexer.hpp"

using namespace math_solver;

// ============================================================
// Helpers
// ============================================================

static Token next_ok(Lexer& l) {
    auto r = l.next_token();
    EXPECT_TRUE(r.ok()) << "expected ok token but got error";
    return r.ok() ? *r : Token{};
}

static void expect_err(Lexer& l) {
    EXPECT_FALSE(l.next_token().ok()) << "expected error token";
}

// ============================================================
// End-of-input
// ============================================================

TEST(MathLexer, EmptyInputIsEnd) {
    Lexer l("");
    EXPECT_EQ(next_ok(l).type, TokenType::End);
}

TEST(MathLexer, WhitespaceOnlyIsEnd) {
    Lexer l("   \t  ");
    EXPECT_EQ(next_ok(l).type, TokenType::End);
}

TEST(MathLexer, EndTokenIsRepeatable) {
    Lexer l("");
    EXPECT_EQ(next_ok(l).type, TokenType::End);
    EXPECT_EQ(next_ok(l).type, TokenType::End);
}

// ============================================================
// Number literals
// ============================================================

TEST(MathLexer, IntegerLiteral) {
    Lexer l("42");
    auto  t = next_ok(l);
    EXPECT_EQ(t.type, TokenType::Number);
    EXPECT_DOUBLE_EQ(t.value, 42.0);
}

TEST(MathLexer, ZeroLiteral) {
    Lexer l("0");
    auto  t = next_ok(l);
    EXPECT_EQ(t.type, TokenType::Number);
    EXPECT_DOUBLE_EQ(t.value, 0.0);
}

TEST(MathLexer, FloatWithDot) {
    Lexer l("3.14");
    auto  t = next_ok(l);
    EXPECT_EQ(t.type, TokenType::Number);
    EXPECT_DOUBLE_EQ(t.value, 3.14);
}

TEST(MathLexer, LeadingDotFloat) {
    Lexer l(".5");
    auto  t = next_ok(l);
    EXPECT_EQ(t.type, TokenType::Number);
    EXPECT_DOUBLE_EQ(t.value, 0.5);
}

TEST(MathLexer, LargeNumber) {
    Lexer l("123456789");
    auto  t = next_ok(l);
    EXPECT_EQ(t.type, TokenType::Number);
    EXPECT_DOUBLE_EQ(t.value, 123456789.0);
}

TEST(MathLexer, LonelyDotIsError) {
    Lexer l(".");
    expect_err(l);
}

// ============================================================
// Identifiers
// ============================================================

TEST(MathLexer, SingleLetterIdentifier) {
    Lexer l("x");
    auto  t = next_ok(l);
    EXPECT_EQ(t.type, TokenType::Identifier);
    EXPECT_EQ(t.name, "x");
}

TEST(MathLexer, MultiLetterIdentifier) {
    Lexer l("abc");
    auto  t = next_ok(l);
    EXPECT_EQ(t.type, TokenType::Identifier);
    EXPECT_EQ(t.name, "abc");
}

TEST(MathLexer, IdentifierWithUnderscore) {
    Lexer l("my_var");
    auto  t = next_ok(l);
    EXPECT_EQ(t.type, TokenType::Identifier);
    EXPECT_EQ(t.name, "my_var");
}

TEST(MathLexer, IdentifierLeadingUnderscore) {
    Lexer l("_x");
    auto  t = next_ok(l);
    EXPECT_EQ(t.type, TokenType::Identifier);
    EXPECT_EQ(t.name, "_x");
}

TEST(MathLexer, IdentifierWithDigitSuffix) {
    Lexer l("x1");
    auto  t = next_ok(l);
    EXPECT_EQ(t.type, TokenType::Identifier);
    EXPECT_EQ(t.name, "x1");
}

// ============================================================
// Reserved keywords → error
// ============================================================

TEST(MathLexer, ReservedKeyword_solve) {
    Lexer l("solve");
    expect_err(l);
}

TEST(MathLexer, ReservedKeyword_expand) {
    Lexer l("expand");
    expect_err(l);
}

TEST(MathLexer, ReservedKeyword_factor) {
    Lexer l("factor");
    expect_err(l);
}

TEST(MathLexer, ReservedKeyword_set) {
    Lexer l("set");
    expect_err(l);
}

TEST(MathLexer, ReservedKeyword_unset) {
    Lexer l("unset");
    expect_err(l);
}

TEST(MathLexer, ReservedKeyword_clear) {
    Lexer l("clear");
    expect_err(l);
}

TEST(MathLexer, ReservedKeyword_config) {
    Lexer l("config");
    expect_err(l);
}

TEST(MathLexer, ReservedKeyword_env) {
    Lexer l("env");
    expect_err(l);
}

// Non-reserved identifiers must succeed
TEST(MathLexer, NonReservedIdentifier_x) {
    Lexer l("x");
    EXPECT_TRUE(l.next_token().ok());
}

TEST(MathLexer, NonReservedIdentifier_result) {
    Lexer l("result");
    EXPECT_TRUE(l.next_token().ok());
}

// ============================================================
// Operators
// ============================================================

TEST(MathLexer, PlusToken)  { Lexer l("+"); EXPECT_EQ(next_ok(l).type, TokenType::Plus); }
TEST(MathLexer, MinusToken) { Lexer l("-"); EXPECT_EQ(next_ok(l).type, TokenType::Minus); }
TEST(MathLexer, MulToken)   { Lexer l("*"); EXPECT_EQ(next_ok(l).type, TokenType::Mul); }
TEST(MathLexer, DivToken)   { Lexer l("/"); EXPECT_EQ(next_ok(l).type, TokenType::Div); }
TEST(MathLexer, PowToken)   { Lexer l("^"); EXPECT_EQ(next_ok(l).type, TokenType::Pow); }
TEST(MathLexer, EqualsToken){ Lexer l("="); EXPECT_EQ(next_ok(l).type, TokenType::Equals); }
TEST(MathLexer, BangToken)  { Lexer l("!"); EXPECT_EQ(next_ok(l).type, TokenType::Bang); }

// ============================================================
// Delimiters
// ============================================================

TEST(MathLexer, LParenToken)   { Lexer l("("); EXPECT_EQ(next_ok(l).type, TokenType::LParen); }
TEST(MathLexer, RParenToken)   { Lexer l(")"); EXPECT_EQ(next_ok(l).type, TokenType::RParen); }
TEST(MathLexer, LBracketToken) { Lexer l("["); EXPECT_EQ(next_ok(l).type, TokenType::LBracket); }
TEST(MathLexer, RBracketToken) { Lexer l("]"); EXPECT_EQ(next_ok(l).type, TokenType::RBracket); }
TEST(MathLexer, CommaToken)    { Lexer l(","); EXPECT_EQ(next_ok(l).type, TokenType::Comma); }

// ============================================================
// Unknown character → error
// ============================================================

TEST(MathLexer, AtSignIsError)   { Lexer l("@"); expect_err(l); }
TEST(MathLexer, HashIsError)     { Lexer l("#"); expect_err(l); }
TEST(MathLexer, AmpIsError)      { Lexer l("&"); expect_err(l); }
TEST(MathLexer, PercentIsError)  { Lexer l("%"); expect_err(l); }

// ============================================================
// Token sequences
// ============================================================

TEST(MathLexer, SimpleExprSequence) {
    Lexer l("x + 2");
    EXPECT_EQ(next_ok(l).type, TokenType::Identifier);
    EXPECT_EQ(next_ok(l).type, TokenType::Plus);
    EXPECT_EQ(next_ok(l).type, TokenType::Number);
    EXPECT_EQ(next_ok(l).type, TokenType::End);
}

TEST(MathLexer, ParenthesisedSequence) {
    Lexer l("(a * b)");
    EXPECT_EQ(next_ok(l).type, TokenType::LParen);
    EXPECT_EQ(next_ok(l).type, TokenType::Identifier);
    EXPECT_EQ(next_ok(l).type, TokenType::Mul);
    EXPECT_EQ(next_ok(l).type, TokenType::Identifier);
    EXPECT_EQ(next_ok(l).type, TokenType::RParen);
    EXPECT_EQ(next_ok(l).type, TokenType::End);
}

TEST(MathLexer, EquationSequence) {
    Lexer l("x + 1 = 0");
    EXPECT_EQ(next_ok(l).type, TokenType::Identifier);
    EXPECT_EQ(next_ok(l).type, TokenType::Plus);
    EXPECT_EQ(next_ok(l).type, TokenType::Number);
    EXPECT_EQ(next_ok(l).type, TokenType::Equals);
    EXPECT_EQ(next_ok(l).type, TokenType::Number);
    EXPECT_EQ(next_ok(l).type, TokenType::End);
}

TEST(MathLexer, ArrayLiteralSequence) {
    Lexer l("[1, 2, 3]");
    EXPECT_EQ(next_ok(l).type, TokenType::LBracket);
    EXPECT_EQ(next_ok(l).type, TokenType::Number);
    EXPECT_EQ(next_ok(l).type, TokenType::Comma);
    EXPECT_EQ(next_ok(l).type, TokenType::Number);
    EXPECT_EQ(next_ok(l).type, TokenType::Comma);
    EXPECT_EQ(next_ok(l).type, TokenType::Number);
    EXPECT_EQ(next_ok(l).type, TokenType::RBracket);
    EXPECT_EQ(next_ok(l).type, TokenType::End);
}

TEST(MathLexer, FunctionCallSequence) {
    Lexer l("sin(x)");
    auto  name = next_ok(l);
    EXPECT_EQ(name.type, TokenType::Identifier);
    EXPECT_EQ(name.name, "sin");
    EXPECT_EQ(next_ok(l).type, TokenType::LParen);
    EXPECT_EQ(next_ok(l).type, TokenType::Identifier);
    EXPECT_EQ(next_ok(l).type, TokenType::RParen);
    EXPECT_EQ(next_ok(l).type, TokenType::End);
}

// ============================================================
// Span tracking
// ============================================================

TEST(MathLexer, IntegerSpan) {
    Lexer l("42");
    auto  t = next_ok(l);
    EXPECT_EQ(t.span.start, 0u);
    EXPECT_EQ(t.span.end,   2u);
}

TEST(MathLexer, IdentifierSpanAfterWhitespace) {
    Lexer l("  xy");
    auto  t = next_ok(l);
    EXPECT_EQ(t.span.start, 2u);
    EXPECT_EQ(t.span.end,   4u);
}

TEST(MathLexer, OperatorSpan) {
    Lexer l("1 + 2");
    next_ok(l);            // consume 1
    auto plus = next_ok(l);
    EXPECT_EQ(plus.span.start, 2u);
    EXPECT_EQ(plus.span.end,   3u);
}

TEST(MathLexer, NumberValueIsCorrect) {
    Lexer l("1 + 2");
    auto  one = next_ok(l);
    EXPECT_DOUBLE_EQ(one.value, 1.0);
    next_ok(l); // +
    auto two = next_ok(l);
    EXPECT_DOUBLE_EQ(two.value, 2.0);
}
