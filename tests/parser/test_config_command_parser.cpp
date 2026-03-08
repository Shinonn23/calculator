#include <gtest/gtest.h>

#include "ast/command/config_command.hpp"
#include "lexer/command/command_token_stream.hpp"
#include "parser/command/subparsers/config_command_parser.hpp"

using namespace math_solver;

// ============================================================
// Action resolution
// ============================================================

TEST(ConfigCommandParser, BareConfigDefaultsList) {
    ConfigCommandParser parser;
    CommandTokenStream  ts(":config");
    auto r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    EXPECT_EQ(static_cast<const ConfigCommand&>(*r->get()).action(),
              ConfigCommand::Action::List);
}

TEST(ConfigCommandParser, ListSubcommand) {
    ConfigCommandParser parser;
    CommandTokenStream  ts(":config list");
    auto r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    EXPECT_EQ(static_cast<const ConfigCommand&>(*r->get()).action(),
              ConfigCommand::Action::List);
}

TEST(ConfigCommandParser, GetSubcommand) {
    ConfigCommandParser parser;
    CommandTokenStream  ts(":config get theme");
    auto r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    auto& cmd = static_cast<const ConfigCommand&>(*r->get());
    EXPECT_EQ(cmd.action(), ConfigCommand::Action::Get);
    EXPECT_EQ(cmd.key(), "theme");
}

TEST(ConfigCommandParser, SetSubcommand) {
    ConfigCommandParser parser;
    CommandTokenStream  ts(":config set theme dark");
    auto r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    auto& cmd = static_cast<const ConfigCommand&>(*r->get());
    EXPECT_EQ(cmd.action(), ConfigCommand::Action::Set);
    EXPECT_EQ(cmd.key(), "theme");
    EXPECT_EQ(cmd.value(), "dark");
}

TEST(ConfigCommandParser, PathSubcommand) {
    ConfigCommandParser parser;
    CommandTokenStream  ts(":config path");
    auto r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    EXPECT_EQ(static_cast<const ConfigCommand&>(*r->get()).action(),
              ConfigCommand::Action::Path);
}

TEST(ConfigCommandParser, ResetSubcommand) {
    ConfigCommandParser parser;
    CommandTokenStream  ts(":config reset");
    auto r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    EXPECT_EQ(static_cast<const ConfigCommand&>(*r->get()).action(),
              ConfigCommand::Action::Reset);
}

TEST(ConfigCommandParser, UnknownSubcommand) {
    ConfigCommandParser parser;
    CommandTokenStream  ts(":config foobar");
    auto r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    auto& cmd = static_cast<const ConfigCommand&>(*r->get());
    EXPECT_EQ(cmd.action(), ConfigCommand::Action::Unknown);
    // Unknown subcommand stored in key field
    EXPECT_EQ(cmd.key(), "foobar");
}

// ============================================================
// Key/value details
// ============================================================

TEST(ConfigCommandParser, GetKeyIsSet) {
    ConfigCommandParser parser;
    CommandTokenStream  ts(":config get precision");
    auto r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    EXPECT_EQ(static_cast<const ConfigCommand&>(*r->get()).key(), "precision");
}

TEST(ConfigCommandParser, SetKeyAndValueAreSet) {
    ConfigCommandParser parser;
    CommandTokenStream  ts(":config set precision 6");
    auto r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    auto& cmd = static_cast<const ConfigCommand&>(*r->get());
    EXPECT_EQ(cmd.key(), "precision");
    EXPECT_EQ(cmd.value(), "6");
}

TEST(ConfigCommandParser, SetValueMultiWord) {
    // Value is the remainder of the input
    ConfigCommandParser parser;
    CommandTokenStream  ts(":config set prompt >>>"); // ">>>" parsed as a word
    auto r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    auto& cmd = static_cast<const ConfigCommand&>(*r->get());
    EXPECT_EQ(cmd.key(), "prompt");
    EXPECT_FALSE(cmd.value().empty());
}

TEST(ConfigCommandParser, ListHasNoKey) {
    ConfigCommandParser parser;
    CommandTokenStream  ts(":config list");
    auto r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    EXPECT_EQ(static_cast<const ConfigCommand&>(*r->get()).key(), "");
}

// ============================================================
// Raw input preserved
// ============================================================

TEST(ConfigCommandParser, RawInputPreserved) {
    ConfigCommandParser parser;
    CommandTokenStream  ts(":config list");
    auto r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    EXPECT_EQ(r->get()->raw_command(), ":config list");
}
