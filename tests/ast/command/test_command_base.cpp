#include <gtest/gtest.h>

#include "ast/command/system_command.hpp"

using namespace math_solver;

// SystemCommand is the simplest concrete Command subclass — used here
// to exercise the base Command interface.

TEST(CommandBaseTest, RawCommandStored) {
    SystemCommand c(SystemCommand::Type::Exit, "exit");
    EXPECT_EQ(c.raw_command(), "exit");
}

TEST(CommandBaseTest, SourceFileDefaultsToRepl) {
    SystemCommand c(SystemCommand::Type::Exit, "exit");
    EXPECT_EQ(c.source_file(), "<repl>");
}

TEST(CommandBaseTest, SourceLineDefaultsToOne) {
    SystemCommand c(SystemCommand::Type::Exit, "exit");
    EXPECT_EQ(c.source_line(), 1u);
}

TEST(CommandBaseTest, SetSourceUpdatesFileAndLine) {
    SystemCommand c(SystemCommand::Type::Exit, "exit");
    c.set_source("script.msl", 42);
    EXPECT_EQ(c.source_file(), "script.msl");
    EXPECT_EQ(c.source_line(), 42u);
}

TEST(CommandBaseTest, SetSourceCanBeCalledMultipleTimes) {
    SystemCommand c(SystemCommand::Type::Exit, "exit");
    c.set_source("a.msl", 1);
    c.set_source("b.msl", 99);
    EXPECT_EQ(c.source_file(), "b.msl");
    EXPECT_EQ(c.source_line(), 99u);
}
