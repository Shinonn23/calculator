#include <gtest/gtest.h>

#include "ast/command/system_command.hpp"
#include "lexer/command/command_token_stream.hpp"
#include "parser/command/subparsers/system_command_parser.hpp"

using namespace math_solver;

// Returns nullptr on non-system input (by design — the registry calls this).
static SystemCommand::Type parse_type(const std::string& input) {
    SystemCommandParser parser;
    CommandTokenStream  ts(input);
    auto                r = parser.parse(ts);
    EXPECT_TRUE(r.ok());
    if (!r.ok() || !r->get()) return SystemCommand::Type::Unknown;
    return static_cast<const SystemCommand&>(*r->get()).type();
}

static bool parse_returns_null(const std::string& input) {
    SystemCommandParser parser;
    CommandTokenStream  ts(input);
    auto                r = parser.parse(ts);
    return r.ok() && r->get() == nullptr;
}

// ============================================================
// Exit variants
// ============================================================

TEST(SystemCommandParser, ExitKeyword)  { EXPECT_EQ(parse_type(":exit"), SystemCommand::Type::Exit); }
TEST(SystemCommandParser, QuitKeyword)  { EXPECT_EQ(parse_type(":quit"), SystemCommand::Type::Exit); }
TEST(SystemCommandParser, QShort)       { EXPECT_EQ(parse_type(":q"),    SystemCommand::Type::Exit); }

// ============================================================
// Help variants
// ============================================================

TEST(SystemCommandParser, HelpKeyword)  { EXPECT_EQ(parse_type(":help"), SystemCommand::Type::Help); }
TEST(SystemCommandParser, HShort)       { EXPECT_EQ(parse_type(":h"),    SystemCommand::Type::Help); }

// ============================================================
// Clear variants
// ============================================================

TEST(SystemCommandParser, ClearKeyword) { EXPECT_EQ(parse_type(":clear"), SystemCommand::Type::Clear); }
TEST(SystemCommandParser, ClsKeyword)   { EXPECT_EQ(parse_type(":cls"),   SystemCommand::Type::Clear); }

// ============================================================
// Ls
// ============================================================

TEST(SystemCommandParser, LsKeyword) { EXPECT_EQ(parse_type(":ls"), SystemCommand::Type::Ls); }

// ============================================================
// Unknown command → returns null CommandPtr (not an error)
// ============================================================

TEST(SystemCommandParser, UnknownCommandReturnsNull) {
    EXPECT_TRUE(parse_returns_null(":unknown_command"));
}

TEST(SystemCommandParser, SetCommandReturnsNull) {
    // :set is handled by VarCommandParser, not SystemCommandParser
    EXPECT_TRUE(parse_returns_null(":set"));
}

// ============================================================
// Raw input is preserved
// ============================================================

TEST(SystemCommandParser, RawInputPreserved) {
    SystemCommandParser parser;
    CommandTokenStream  ts(":exit");
    auto                r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    ASSERT_NE(r->get(), nullptr);
    EXPECT_EQ(r->get()->raw_command(), ":exit");
}
