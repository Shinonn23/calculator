#include <gtest/gtest.h>

#include "ast/command/system_command.hpp"
#include "ast/mock_visitors.hpp"
#include "diagnostics/sink.hpp"

using namespace math_solver;
using test_helpers::MockCommandVisitor;

TEST(SystemCommandTest, TypeExit) {
    EXPECT_EQ(SystemCommand(SystemCommand::Type::Exit, "exit").type(),
              SystemCommand::Type::Exit);
}

TEST(SystemCommandTest, TypeHelp) {
    EXPECT_EQ(SystemCommand(SystemCommand::Type::Help, "help").type(),
              SystemCommand::Type::Help);
}

TEST(SystemCommandTest, TypeClear) {
    EXPECT_EQ(SystemCommand(SystemCommand::Type::Clear, "clear").type(),
              SystemCommand::Type::Clear);
}

TEST(SystemCommandTest, TypeLs) {
    EXPECT_EQ(SystemCommand(SystemCommand::Type::Ls, "ls").type(),
              SystemCommand::Type::Ls);
}

TEST(SystemCommandTest, TypeUnknown) {
    EXPECT_EQ(SystemCommand(SystemCommand::Type::Unknown, "???").type(),
              SystemCommand::Type::Unknown);
}

TEST(SystemCommandTest, AcceptDispatchesSystemCommand) {
    SystemCommand      c(SystemCommand::Type::Exit, "exit");
    MockCommandVisitor v;
    DiagnosticSink     sink;
    c.accept(v, sink);
    EXPECT_EQ(v.last, MockCommandVisitor::Visited::System);
}
