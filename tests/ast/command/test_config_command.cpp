#include <gtest/gtest.h>

#include "ast/command/config_command.hpp"
#include "ast/mock_visitors.hpp"
#include "diagnostics/sink.hpp"

using namespace math_solver;
using test_helpers::MockCommandVisitor;

TEST(ConfigCommandTest, AllActionsConstructible) {
    EXPECT_EQ(ConfigCommand(ConfigCommand::Action::List,    "").action(), ConfigCommand::Action::List);
    EXPECT_EQ(ConfigCommand(ConfigCommand::Action::Get,     "").action(), ConfigCommand::Action::Get);
    EXPECT_EQ(ConfigCommand(ConfigCommand::Action::Set,     "").action(), ConfigCommand::Action::Set);
    EXPECT_EQ(ConfigCommand(ConfigCommand::Action::Path,    "").action(), ConfigCommand::Action::Path);
    EXPECT_EQ(ConfigCommand(ConfigCommand::Action::Reset,   "").action(), ConfigCommand::Action::Reset);
    EXPECT_EQ(ConfigCommand(ConfigCommand::Action::Unknown, "").action(), ConfigCommand::Action::Unknown);
}

TEST(ConfigCommandTest, KeyAndValueDefaultEmpty) {
    ConfigCommand c(ConfigCommand::Action::List, "config list");
    EXPECT_TRUE(c.key().empty());
    EXPECT_TRUE(c.value().empty());
}

TEST(ConfigCommandTest, SetKvKeyOnly) {
    ConfigCommand c(ConfigCommand::Action::Get, "config get theme");
    c.set_kv("theme");
    EXPECT_EQ(c.key(), "theme");
    EXPECT_TRUE(c.value().empty());
}

TEST(ConfigCommandTest, SetKvKeyAndValue) {
    ConfigCommand c(ConfigCommand::Action::Set, "config set theme dark");
    c.set_kv("theme", "dark");
    EXPECT_EQ(c.key(),   "theme");
    EXPECT_EQ(c.value(), "dark");
}

TEST(ConfigCommandTest, SetKvOverwritesPreviousValues) {
    ConfigCommand c(ConfigCommand::Action::Set, "config set k v");
    c.set_kv("first", "one");
    c.set_kv("second", "two");
    EXPECT_EQ(c.key(),   "second");
    EXPECT_EQ(c.value(), "two");
}

TEST(ConfigCommandTest, AcceptDispatchesConfigCommand) {
    ConfigCommand      c(ConfigCommand::Action::List, "config list");
    MockCommandVisitor v;
    DiagnosticSink     sink;
    c.accept(v, sink);
    EXPECT_EQ(v.last, MockCommandVisitor::Visited::Config);
}
