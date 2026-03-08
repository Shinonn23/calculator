#include <gtest/gtest.h>
#include <vector>

#include "ast/command/var_command.hpp"
#include "ast/mock_visitors.hpp"
#include "diagnostics/sink.hpp"

using namespace math_solver;
using test_helpers::MockCommandVisitor;

TEST(VarCommandTest, ActionSet) {
    EXPECT_EQ(VarCommand(VarCommand::Action::Set, std::vector<std::string>{"x"},
                         "var x = 1")
                  .action(),
              VarCommand::Action::Set);
}

TEST(VarCommandTest, ActionUnset) {
    EXPECT_EQ(VarCommand(VarCommand::Action::Unset,
                         std::vector<std::string>{"x"}, "var x unset")
                  .action(),
              VarCommand::Action::Unset);
}

TEST(VarCommandTest, VarNameUnset) {
    std::vector<std::string> names{"x", "y", "z"};
    EXPECT_EQ(VarCommand(VarCommand::Action::Unset, names, "var x, y, z unset")
                  .var_name(),
              names);
}

TEST(VarCommandTest, ActionUnknown) {
    EXPECT_EQ(VarCommand(VarCommand::Action::Unknown,
                         std::vector<std::string>{"x"}, "var x ?")
                  .action(),
              VarCommand::Action::Unknown);
}

TEST(VarCommandTest, VarNameStored) {
    EXPECT_EQ(VarCommand(VarCommand::Action::Set,
                         std::vector<std::string>{"myVar"}, "var myVar = 3")
                  .var_name(),
              std::vector<std::string>{"myVar"});
}

TEST(VarCommandTest, NoPayloadByDefault) {
    VarCommand c(VarCommand::Action::Set, std::vector<std::string>{"x"},
                 "var x");
    EXPECT_FALSE(c.has_payload());
    EXPECT_FALSE(c.has_math_action());
}

TEST(VarCommandTest, SetPayloadStoresMathActionAndPayload) {
    VarCommand c(VarCommand::Action::Set, std::vector<std::string>{"x"},
                 "var x solve x+1=0");
    c.set_payload("solve", "x+1=0");
    EXPECT_TRUE(c.has_payload());
    EXPECT_TRUE(c.has_math_action());
    EXPECT_EQ(c.payload(), "x+1=0");
    EXPECT_EQ(c.math_action(), "solve");
}

TEST(VarCommandTest, AcceptDispatchesVarCommand) {
    VarCommand         c(VarCommand::Action::Set, std::vector<std::string>{"x"},
                         "var x = 1");
    MockCommandVisitor v;
    DiagnosticSink     sink;
    c.accept(v, sink);
    EXPECT_EQ(v.last, MockCommandVisitor::Visited::Var);
}
