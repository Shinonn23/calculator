// ============================================================
// Unit tests สำหรับ Lexer และ Token
// ครอบคลุม: tokenisation ทุกประเภท, error cases, token_type_name,
//           is_reserved_keyword
// ============================================================

#include "common/error.h"
#include "lexer/lexer.h"
#include "lexer/token.h"
#include <gtest/gtest.h>

using namespace math_solver;
using namespace std;

// ============================================================
// token_type_name tests
// ============================================================

// ทดสอบชื่อ token ทุกประเภท
TEST(TokenTypeNameTest, AllTypes) {
    EXPECT_STREQ(token_type_name(TokenType::End), "end of input");
    EXPECT_STREQ(token_type_name(TokenType::Number), "number");
    EXPECT_STREQ(token_type_name(TokenType::Identifier), "identifier");
    EXPECT_STREQ(token_type_name(TokenType::Plus), "+");
    EXPECT_STREQ(token_type_name(TokenType::Minus), "-");
    EXPECT_STREQ(token_type_name(TokenType::Mul), "*");
    EXPECT_STREQ(token_type_name(TokenType::Div), "/");
    EXPECT_STREQ(token_type_name(TokenType::Pow), "^");
    EXPECT_STREQ(token_type_name(TokenType::LParen), "(");
    EXPECT_STREQ(token_type_name(TokenType::RParen), ")");
    EXPECT_STREQ(token_type_name(TokenType::LBracket), "[");
    EXPECT_STREQ(token_type_name(TokenType::RBracket), "]");
    EXPECT_STREQ(token_type_name(TokenType::LBrace), "{");
    EXPECT_STREQ(token_type_name(TokenType::RBrace), "}");
    EXPECT_STREQ(token_type_name(TokenType::Comma), ",");
    EXPECT_STREQ(token_type_name(TokenType::Equals), "=");
}

// ============================================================
// is_reserved_keyword tests
// ============================================================

// ทดสอบ keywords ที่ถูก reserve
TEST(IsReservedKeywordTest, ReservedWords) {
    EXPECT_TRUE(is_reserved_keyword("simplify"));
    EXPECT_TRUE(is_reserved_keyword("solve"));
    EXPECT_TRUE(is_reserved_keyword("set"));
    EXPECT_TRUE(is_reserved_keyword("unset"));
    EXPECT_TRUE(is_reserved_keyword("clear"));
    EXPECT_TRUE(is_reserved_keyword("help"));
    EXPECT_TRUE(is_reserved_keyword("exit"));
    EXPECT_TRUE(is_reserved_keyword("quit"));
    EXPECT_TRUE(is_reserved_keyword("print"));
    EXPECT_TRUE(is_reserved_keyword("let"));
}

// ทดสอบ keywords ที่ไม่ถูก reserve
TEST(IsReservedKeywordTest, NotReserved) {
    EXPECT_FALSE(is_reserved_keyword("x"));
    EXPECT_FALSE(is_reserved_keyword("abc"));
    EXPECT_FALSE(is_reserved_keyword("my_var"));
    EXPECT_FALSE(is_reserved_keyword(""));
}

// ============================================================
// Token default constructor tests
// ============================================================

// ทดสอบ default constructor — type=End, value=0
TEST(TokenTest, DefaultConstructor) {
    Token t;
    EXPECT_EQ(t.type, TokenType::End);
    EXPECT_DOUBLE_EQ(t.value, 0.0);
    EXPECT_TRUE(t.name.empty());
}

// ทดสอบ constructor กับ numeric value
TEST(TokenTest, NumericConstructor) {
    Token t(TokenType::Number, 42.0, Span(0, 2));
    EXPECT_EQ(t.type, TokenType::Number);
    EXPECT_DOUBLE_EQ(t.value, 42.0);
    EXPECT_EQ(t.span.start, 0u);
}

// ทดสอบ constructor กับ string name
TEST(TokenTest, IdentifierConstructor) {
    Token t(TokenType::Identifier, "xyz", Span(0, 3));
    EXPECT_EQ(t.type, TokenType::Identifier);
    EXPECT_EQ(t.name, "xyz");
}

// ============================================================
// Lexer — basic tokenisation
// ============================================================

// ทดสอบ lexing ตัวเลขจำนวนเต็ม
TEST(LexerTest, IntegerNumber) {
    Lexer lexer("42");
    Token t = lexer.next_token();
    EXPECT_EQ(t.type, TokenType::Number);
    EXPECT_DOUBLE_EQ(t.value, 42.0);

    Token end = lexer.next_token();
    EXPECT_EQ(end.type, TokenType::End);
}

// ทดสอบ lexing ทศนิยม
TEST(LexerTest, DecimalNumber) {
    Lexer lexer("3.14");
    Token t = lexer.next_token();
    EXPECT_EQ(t.type, TokenType::Number);
    EXPECT_DOUBLE_EQ(t.value, 3.14);
}

// ทดสอบ lexing identifier (ตัวแปร)
TEST(LexerTest, Identifier) {
    Lexer lexer("xyz");
    Token t = lexer.next_token();
    EXPECT_EQ(t.type, TokenType::Identifier);
    EXPECT_EQ(t.name, "xyz");
}

// ทดสอบ identifier ที่มี underscore
TEST(LexerTest, IdentifierWithUnderscore) {
    Lexer lexer("my_var");
    Token t = lexer.next_token();
    EXPECT_EQ(t.type, TokenType::Identifier);
    EXPECT_EQ(t.name, "my_var");
}

// ทดสอบ identifier ที่ขึ้นต้นด้วย underscore
TEST(LexerTest, IdentifierStartsWithUnderscore) {
    Lexer lexer("_foo");
    Token t = lexer.next_token();
    EXPECT_EQ(t.type, TokenType::Identifier);
    EXPECT_EQ(t.name, "_foo");
}

// ทดสอบ operators ทั้งหมด
TEST(LexerTest, AllOperators) {
    Lexer lexer("+ - * / ^ ( ) =");

    EXPECT_EQ(lexer.next_token().type, TokenType::Plus);
    EXPECT_EQ(lexer.next_token().type, TokenType::Minus);
    EXPECT_EQ(lexer.next_token().type, TokenType::Mul);
    EXPECT_EQ(lexer.next_token().type, TokenType::Div);
    EXPECT_EQ(lexer.next_token().type, TokenType::Pow);
    EXPECT_EQ(lexer.next_token().type, TokenType::LParen);
    EXPECT_EQ(lexer.next_token().type, TokenType::RParen);
    EXPECT_EQ(lexer.next_token().type, TokenType::Equals);
    EXPECT_EQ(lexer.next_token().type, TokenType::End);
}

// ทดสอบ empty input
TEST(LexerTest, EmptyInput) {
    Lexer lexer("");
    Token t = lexer.next_token();
    EXPECT_EQ(t.type, TokenType::End);
}

// ทดสอบ whitespace only
TEST(LexerTest, WhitespaceOnly) {
    Lexer lexer("   \t\n  ");
    Token t = lexer.next_token();
    EXPECT_EQ(t.type, TokenType::End);
}

// ทดสอบ expression ที่รวมหลายประเภท: "2 + x * 3"
TEST(LexerTest, MixedExpression) {
    Lexer lexer("2 + x * 3");

    Token t1 = lexer.next_token();
    EXPECT_EQ(t1.type, TokenType::Number);
    EXPECT_DOUBLE_EQ(t1.value, 2.0);

    Token t2 = lexer.next_token();
    EXPECT_EQ(t2.type, TokenType::Plus);

    Token t3 = lexer.next_token();
    EXPECT_EQ(t3.type, TokenType::Identifier);
    EXPECT_EQ(t3.name, "x");

    Token t4 = lexer.next_token();
    EXPECT_EQ(t4.type, TokenType::Mul);

    Token t5 = lexer.next_token();
    EXPECT_EQ(t5.type, TokenType::Number);
    EXPECT_DOUBLE_EQ(t5.value, 3.0);

    EXPECT_EQ(lexer.next_token().type, TokenType::End);
}

// ทดสอบ Span ที่ถูกต้องของ token
TEST(LexerTest, TokenSpans) {
    Lexer lexer("12 + x");

    Token t1 = lexer.next_token(); // "12" at [0,2)
    EXPECT_EQ(t1.span.start, 0u);
    EXPECT_EQ(t1.span.end, 2u);

    Token t2 = lexer.next_token(); // "+" at [3,4)
    EXPECT_EQ(t2.span.start, 3u);
    EXPECT_EQ(t2.span.end, 4u);

    Token t3 = lexer.next_token(); // "x" at [5,6)
    EXPECT_EQ(t3.span.start, 5u);
    EXPECT_EQ(t3.span.end, 6u);
}

// ทดสอบ Lexer.input() accessor
TEST(LexerTest, InputAccessor) {
    Lexer lexer("hello");
    EXPECT_EQ(lexer.input(), "hello");
}

// ทดสอบ Lexer.position() หลังอ่าน token
TEST(LexerTest, PositionAfterTokens) {
    Lexer lexer("ab");
    EXPECT_EQ(lexer.position(), 0u);
    lexer.next_token(); // อ่าน "ab"
    EXPECT_EQ(lexer.position(), 2u);
}

// ============================================================
// Lexer — error cases
// ============================================================

// ทดสอบ unexpected character (เช่น @)
TEST(LexerTest, UnexpectedCharacter) {
    Lexer lexer("@");
    EXPECT_THROW(lexer.next_token(), ParseError);
}

// ทดสอบ # เป็น comment — skip ทั้งบรรทัด return End
TEST(LexerTest, CommentIsSkipped) {
    Lexer lexer("#");
    Token t = lexer.next_token();
    EXPECT_EQ(t.type, TokenType::End);
}

// ทดสอบ reserved keyword ถูก reject
TEST(LexerTest, ReservedKeywordThrows) {
    Lexer lexer("solve");
    EXPECT_THROW(lexer.next_token(), ReservedKeywordError);
}

// ทดสอบ reserved keyword แต่ละตัว
TEST(LexerTest, AllReservedKeywordsThrow) {
    vector<string> keywords = {"simplify", "solve", "set",  "unset",
                               "clear",    "help",  "exit", "quit"};
    for (const auto& kw : keywords) {
        Lexer lexer(kw);
        EXPECT_THROW(lexer.next_token(), ReservedKeywordError)
            << "Should throw for keyword: " << kw;
    }
}

// ทดสอบ lexing ตัวเลขที่ไม่มี whitespace ก่อน identifier: "2x"
TEST(LexerTest, NumberFollowedByIdentifier) {
    Lexer lexer("2x");

    Token t1 = lexer.next_token();
    EXPECT_EQ(t1.type, TokenType::Number);
    EXPECT_DOUBLE_EQ(t1.value, 2.0);

    Token t2 = lexer.next_token();
    EXPECT_EQ(t2.type, TokenType::Identifier);
    EXPECT_EQ(t2.name, "x");
}

// ทดสอบ ตัวเลขที่ขึ้นต้นด้วย dot เช่น ".5"
TEST(LexerTest, DotOnlyNumber) {
    Lexer lexer(".5");
    Token t = lexer.next_token();
    EXPECT_EQ(t.type, TokenType::Number);
    EXPECT_DOUBLE_EQ(t.value, 0.5);
}

// ทดสอบ ตัวเลขที่มี trailing dot เช่น "5."
TEST(LexerTest, TrailingDotNumber) {
    Lexer lexer("5.");
    Token t = lexer.next_token();
    EXPECT_EQ(t.type, TokenType::Number);
    EXPECT_DOUBLE_EQ(t.value, 5.0);
}

// ============================================================
// Comment tests (#)
// ============================================================

// ทดสอบ full-line comment
TEST(LexerTest, FullLineComment) {
    Lexer lexer("# this is a comment");
    Token t = lexer.next_token();
    EXPECT_EQ(t.type, TokenType::End);
}

// ทดสอบ inline comment — code ก่อน #
TEST(LexerTest, InlineComment) {
    Lexer lexer("2 + 3 # trailing comment");
    Token t1 = lexer.next_token();
    EXPECT_EQ(t1.type, TokenType::Number);
    EXPECT_DOUBLE_EQ(t1.value, 2.0);

    Token t2 = lexer.next_token();
    EXPECT_EQ(t2.type, TokenType::Plus);

    Token t3 = lexer.next_token();
    EXPECT_EQ(t3.type, TokenType::Number);
    EXPECT_DOUBLE_EQ(t3.value, 3.0);

    Token t4 = lexer.next_token();
    EXPECT_EQ(t4.type, TokenType::End); // comment ignored
}

// ทดสอบ empty comment
TEST(LexerTest, EmptyComment) {
    Lexer lexer("#");
    Token t = lexer.next_token();
    EXPECT_EQ(t.type, TokenType::End);
}

// ทดสอบ comment หลัง whitespace
TEST(LexerTest, CommentAfterWhitespace) {
    Lexer lexer("   # indented comment");
    Token t = lexer.next_token();
    EXPECT_EQ(t.type, TokenType::End);
}

// ============================================================
// New token type tests (brackets, braces, comma)
// ============================================================

TEST(LexerTest, BracketTokens) {
    Lexer lexer("[0]");
    Token t1 = lexer.next_token();
    EXPECT_EQ(t1.type, TokenType::LBracket);
    Token t2 = lexer.next_token();
    EXPECT_EQ(t2.type, TokenType::Number);
    Token t3 = lexer.next_token();
    EXPECT_EQ(t3.type, TokenType::RBracket);
}

TEST(LexerTest, BraceTokens) {
    Lexer lexer("{ }");
    Token t1 = lexer.next_token();
    EXPECT_EQ(t1.type, TokenType::LBrace);
    Token t2 = lexer.next_token();
    EXPECT_EQ(t2.type, TokenType::RBrace);
}

TEST(LexerTest, CommaToken) {
    Lexer lexer("a, b");
    Token t1 = lexer.next_token();
    EXPECT_EQ(t1.type, TokenType::Identifier);
    Token t2 = lexer.next_token();
    EXPECT_EQ(t2.type, TokenType::Comma);
    Token t3 = lexer.next_token();
    EXPECT_EQ(t3.type, TokenType::Identifier);
}
