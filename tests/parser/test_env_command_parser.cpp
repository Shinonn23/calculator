#include <gtest/gtest.h>

#include "ast/command/env_command.hpp"
#include "lexer/command/command_token_stream.hpp"
#include "parser/command/subparsers/env_command_parser.hpp"

using namespace math_solver;

// ============================================================
// Action resolution
// ============================================================

TEST(EnvCommandParser, BareEnvDefaultsShow) {
    EnvCommandParser   parser;
    CommandTokenStream ts(":env");
    auto r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    EXPECT_EQ(static_cast<const EnvCommand&>(*r->get()).action(),
              EnvCommand::Action::Show);
}

TEST(EnvCommandParser, ListSubcommand) {
    EnvCommandParser   parser;
    CommandTokenStream ts(":env list");
    auto r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    EXPECT_EQ(static_cast<const EnvCommand&>(*r->get()).action(),
              EnvCommand::Action::List);
}

TEST(EnvCommandParser, LsAliasForList) {
    EnvCommandParser   parser;
    CommandTokenStream ts(":env ls");
    auto r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    EXPECT_EQ(static_cast<const EnvCommand&>(*r->get()).action(),
              EnvCommand::Action::List);
}

TEST(EnvCommandParser, LoadSubcommand) {
    EnvCommandParser   parser;
    CommandTokenStream ts(":env load work");
    auto r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    auto& cmd = static_cast<const EnvCommand&>(*r->get());
    EXPECT_EQ(cmd.action(), EnvCommand::Action::Load);
    EXPECT_EQ(cmd.target_env(), "work");
}

TEST(EnvCommandParser, NewSubcommand) {
    EnvCommandParser   parser;
    CommandTokenStream ts(":env new myenv");
    auto r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    auto& cmd = static_cast<const EnvCommand&>(*r->get());
    EXPECT_EQ(cmd.action(), EnvCommand::Action::New);
    EXPECT_EQ(cmd.target_env(), "myenv");
}

TEST(EnvCommandParser, DeleteSubcommand) {
    EnvCommandParser   parser;
    CommandTokenStream ts(":env delete old");
    auto r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    auto& cmd = static_cast<const EnvCommand&>(*r->get());
    EXPECT_EQ(cmd.action(), EnvCommand::Action::Delete);
    EXPECT_EQ(cmd.target_env(), "old");
}

// ============================================================
// Save subcommand
// ============================================================

TEST(EnvCommandParser, SaveWithTarget) {
    EnvCommandParser   parser;
    CommandTokenStream ts(":env save work");
    auto r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    auto& cmd = static_cast<const EnvCommand&>(*r->get());
    EXPECT_EQ(cmd.action(), EnvCommand::Action::Save);
    EXPECT_EQ(cmd.target_env(), "work");
}

TEST(EnvCommandParser, SaveWithVarsSubset) {
    EnvCommandParser   parser;
    CommandTokenStream ts(":env save work --vars x y");
    auto r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    auto& cmd = static_cast<const EnvCommand&>(*r->get());
    EXPECT_EQ(cmd.action(), EnvCommand::Action::Save);
    ASSERT_EQ(cmd.vars_to_save().size(), 2u);
    EXPECT_EQ(cmd.vars_to_save()[0], "x");
    EXPECT_EQ(cmd.vars_to_save()[1], "y");
}

TEST(EnvCommandParser, SaveWithoutTargetAndNoVars) {
    EnvCommandParser   parser;
    CommandTokenStream ts(":env save");
    auto r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    auto& cmd = static_cast<const EnvCommand&>(*r->get());
    EXPECT_EQ(cmd.action(), EnvCommand::Action::Save);
    EXPECT_TRUE(cmd.vars_to_save().empty());
}

// ============================================================
// Move subcommand
// ============================================================

TEST(EnvCommandParser, MoveEnvMode) {
    EnvCommandParser   parser;
    CommandTokenStream ts(":env move src dst");
    auto r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    auto& cmd = static_cast<const EnvCommand&>(*r->get());
    EXPECT_EQ(cmd.action(), EnvCommand::Action::Move);
    EXPECT_EQ(cmd.source_env(), "src");
    EXPECT_EQ(cmd.target_env(), "dst");
    EXPECT_FALSE(cmd.flags().vars_mode);
}

TEST(EnvCommandParser, MvAliasForMove) {
    EnvCommandParser   parser;
    CommandTokenStream ts(":env mv src dst");
    auto r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    EXPECT_EQ(static_cast<const EnvCommand&>(*r->get()).action(),
              EnvCommand::Action::Move);
}

TEST(EnvCommandParser, MoveVarsMode) {
    EnvCommandParser   parser;
    CommandTokenStream ts(":env move --vars x y --to dst");
    auto r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    auto& cmd = static_cast<const EnvCommand&>(*r->get());
    EXPECT_EQ(cmd.action(), EnvCommand::Action::Move);
    EXPECT_TRUE(cmd.flags().vars_mode);
    EXPECT_EQ(cmd.flags().to_env, "dst");
    ASSERT_EQ(cmd.vars_to_save().size(), 2u);
    EXPECT_EQ(cmd.vars_to_save()[0], "x");
    EXPECT_EQ(cmd.vars_to_save()[1], "y");
}

// ============================================================
// Copy subcommand
// ============================================================

TEST(EnvCommandParser, CopyEnvMode) {
    EnvCommandParser   parser;
    CommandTokenStream ts(":env copy a b");
    auto r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    auto& cmd = static_cast<const EnvCommand&>(*r->get());
    EXPECT_EQ(cmd.action(), EnvCommand::Action::Copy);
    EXPECT_EQ(cmd.source_env(), "a");
    EXPECT_EQ(cmd.target_env(), "b");
}

TEST(EnvCommandParser, CpAliasForCopy) {
    EnvCommandParser   parser;
    CommandTokenStream ts(":env cp a b");
    auto r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    EXPECT_EQ(static_cast<const EnvCommand&>(*r->get()).action(),
              EnvCommand::Action::Copy);
}

TEST(EnvCommandParser, CopyVarsMode) {
    EnvCommandParser   parser;
    CommandTokenStream ts(":env copy --vars a --to dst");
    auto r = parser.parse(ts);
    ASSERT_TRUE(r.ok());
    auto& cmd = static_cast<const EnvCommand&>(*r->get());
    EXPECT_TRUE(cmd.flags().vars_mode);
    EXPECT_EQ(cmd.flags().to_env, "dst");
}

// ============================================================
// Unknown subcommand → error
// ============================================================

TEST(EnvCommandParser, UnknownSubcommandIsError) {
    EnvCommandParser   parser;
    CommandTokenStream ts(":env foobar");
    EXPECT_FALSE(parser.parse(ts).ok());
}
