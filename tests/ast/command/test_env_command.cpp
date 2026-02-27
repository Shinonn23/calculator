#include <gtest/gtest.h>

#include "ast/command/env_command.hpp"
#include "ast/mock_visitors.hpp"
#include "diagnostics/sink.hpp"

using namespace math_solver;
using test_helpers::MockCommandVisitor;

TEST(EnvCommandTest, AllActionsConstructible) {
    for (auto a : {EnvCommand::Action::Show,    EnvCommand::Action::List,
                   EnvCommand::Action::Load,    EnvCommand::Action::Save,
                   EnvCommand::Action::New,     EnvCommand::Action::Delete,
                   EnvCommand::Action::Move,    EnvCommand::Action::Copy,
                   EnvCommand::Action::Unknown}) {
        EXPECT_EQ(EnvCommand(a, "", "").action(), a);
    }
}

TEST(EnvCommandTest, TargetEnvStored) {
    EnvCommand c(EnvCommand::Action::Load, "work", "env load work");
    EXPECT_EQ(c.target_env(), "work");
}

TEST(EnvCommandTest, SourceEnvDefaultEmpty) {
    EnvCommand c(EnvCommand::Action::Move, "dst", "env move src dst");
    EXPECT_TRUE(c.source_env().empty());
}

TEST(EnvCommandTest, SetSourceEnv) {
    EnvCommand c(EnvCommand::Action::Move, "dst", "env move src dst");
    c.set_source_env("src");
    EXPECT_EQ(c.source_env(), "src");
}

TEST(EnvCommandTest, VarsToSaveEmptyByDefault) {
    EnvCommand c(EnvCommand::Action::Save, "env1", "env save env1");
    EXPECT_TRUE(c.vars_to_save().empty());
}

TEST(EnvCommandTest, SetVarsToSave) {
    EnvCommand c(EnvCommand::Action::Save, "env1", "env save env1");
    c.set_vars_to_save({"x", "y", "z"});
    ASSERT_EQ(c.vars_to_save().size(), 3u);
    EXPECT_EQ(c.vars_to_save()[0], "x");
    EXPECT_EQ(c.vars_to_save()[2], "z");
}

TEST(EnvCommandTest, FlagsDefaultOff) {
    EnvCommand c(EnvCommand::Action::Copy, "dst", "env copy src dst");
    EXPECT_FALSE(c.flags().vars_mode);
    EXPECT_TRUE(c.flags().to_env.empty());
}

TEST(EnvCommandTest, SetFlags) {
    EnvCommand c(EnvCommand::Action::Move, "dst", "env move src dst");
    c.set_flags({true, "other"});
    EXPECT_TRUE(c.flags().vars_mode);
    EXPECT_EQ(c.flags().to_env, "other");
}

TEST(EnvCommandTest, AcceptDispatchesEnvCommand) {
    EnvCommand         c(EnvCommand::Action::List, "", "env list");
    MockCommandVisitor v;
    DiagnosticSink     sink;
    c.accept(v, sink);
    EXPECT_EQ(v.last, MockCommandVisitor::Visited::Env);
}
