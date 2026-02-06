#include "../src/common/command.h"
#include "../src/common/console_ui.h"
#include "../src/eval/context.h"
#include <gtest/gtest.h>
#include <sstream>

using namespace math_solver;
using namespace std;

// Shared ConsoleUI for tests - routes both out and err to cout
// so CaptureStdout() catches everything
namespace {
    ConsoleUI& test_ui() {
        static ConsoleUI ui(cout, cout);
        return ui;
    }
} // namespace

// ============================================================
// Existing tests (preserved from the original file)
// ============================================================

TEST(CommandParseFlagsTest, NoFlags) {
    string       input = "x + y = 5";
    CommandFlags flags = parse_flags(input);

    EXPECT_EQ(flags.expression, "x + y = 5");
    EXPECT_TRUE(flags.vars.empty());
    EXPECT_FALSE(flags.isolated);
    EXPECT_FALSE(flags.fraction);
}

TEST(CommandParseFlagsTest, WithFlags) {
    string       input = "x + y = 5 --vars x y --isolated --fraction";
    CommandFlags flags = parse_flags(input);

    EXPECT_EQ(flags.expression, "x + y = 5");
    EXPECT_EQ(flags.vars.size(), 2);
    EXPECT_EQ(flags.vars[0], "x");
    EXPECT_EQ(flags.vars[1], "y");
    EXPECT_TRUE(flags.isolated);
    EXPECT_TRUE(flags.fraction);
}

TEST(CommandParseFlagsTest, FlagsOnly) {
    string       input = "--vars a b c --fraction";
    CommandFlags flags = parse_flags(input);

    EXPECT_EQ(flags.expression, "");
    EXPECT_EQ(flags.vars.size(), 3);
    EXPECT_EQ(flags.vars[0], "a");
    EXPECT_EQ(flags.vars[1], "b");
    EXPECT_EQ(flags.vars[2], "c");
    EXPECT_FALSE(flags.isolated);
    EXPECT_TRUE(flags.fraction);
}

TEST(CommandParseFlagsTest, MixedOrderFlags) {
    string input = "solve this equation --fraction --vars x y z --isolated";
    CommandFlags flags = parse_flags(input);

    EXPECT_EQ(flags.expression, "solve this equation");
    EXPECT_EQ(flags.vars.size(), 3);
    EXPECT_EQ(flags.vars[0], "x");
    EXPECT_EQ(flags.vars[1], "y");
    EXPECT_EQ(flags.vars[2], "z");
    EXPECT_TRUE(flags.isolated);
    EXPECT_TRUE(flags.fraction);
}

TEST(CommandParseFlagsTest, NoExpressionWithFlags) {
    string       input = "--vars m n --isolated ";
    CommandFlags flags = parse_flags(input);

    EXPECT_EQ(flags.expression, "");
    EXPECT_EQ(flags.vars.size(), 2);
    EXPECT_EQ(flags.vars[0], "m");
    EXPECT_EQ(flags.vars[1], "n");
    EXPECT_TRUE(flags.isolated);
    EXPECT_FALSE(flags.fraction);
}

TEST(CommandParseFlagsTest, ExtraSpaces) {
    string       input = "   a + b = c    --vars   a   b   --fraction   ";
    CommandFlags flags = parse_flags(input);

    EXPECT_EQ(flags.expression, "a + b = c");
    EXPECT_EQ(flags.vars.size(), 2);
    EXPECT_EQ(flags.vars[0], "a");
    EXPECT_EQ(flags.vars[1], "b");
    EXPECT_FALSE(flags.isolated);
    EXPECT_TRUE(flags.fraction);
}

TEST(CommandParseFlagsTest, NoFlagsButDashesInExpression) {
    string       input = "x - y = 10 - 5";
    CommandFlags flags = parse_flags(input);

    EXPECT_EQ(flags.expression, "x - y = 10 - 5");
    EXPECT_TRUE(flags.vars.empty());
    EXPECT_FALSE(flags.isolated);
    EXPECT_FALSE(flags.fraction);
}

TEST(CmdSet, ValidVariable) {
    Context ctx;
    cmd_set("x 42", ctx, test_ui());

    EXPECT_TRUE(ctx.has("x"));
    EXPECT_DOUBLE_EQ(ctx.get("x"), 42.0);
}

TEST(CmdSet, MissingArguments) {
    Context ctx;

    testing::internal::CaptureStdout();
    cmd_set("my_var", ctx, test_ui());
    string output = testing::internal::GetCapturedStdout();
    EXPECT_TRUE(output.find("set <variable> <value>") != string::npos);
    EXPECT_FALSE(ctx.has("my_var"));

    testing::internal::CaptureStdout();
    cmd_set("", ctx, test_ui());
    output = testing::internal::GetCapturedStdout();
    EXPECT_TRUE(output.find("set <variable> <value>") != string::npos);
    EXPECT_TRUE(ctx.empty());
}

TEST(CmdSet, InvalidVariableName) {
    Context ctx;

    testing::internal::CaptureStdout();
    cmd_set("1x 10", ctx, test_ui());
    string output = testing::internal::GetCapturedStdout();
    EXPECT_TRUE(output.find("invalid variable name") != string::npos);
    EXPECT_FALSE(ctx.has("1x"));

    testing::internal::CaptureStdout();
    cmd_set("var! 5", ctx, test_ui());
    output = testing::internal::GetCapturedStdout();
    EXPECT_TRUE(output.find("invalid variable name") != string::npos);
    EXPECT_FALSE(ctx.has("var!"));
}

TEST(CmdSet, ReservedKeyword) {
    Context ctx;

    testing::internal::CaptureStdout();
    cmd_set("solve 10", ctx, test_ui());
    string output = testing::internal::GetCapturedStdout();
    EXPECT_TRUE(output.find("reserved keyword") != string::npos);
    EXPECT_FALSE(ctx.has("solve"));
}

TEST(CmdUnset, ExistingVariable) {
    Context ctx;
    ctx.set("y", 3.14);

    testing::internal::CaptureStdout();
    cmd_unset("y", ctx, test_ui());
    string output = testing::internal::GetCapturedStdout();
    EXPECT_TRUE(output.find("Removed: y") != string::npos);
    EXPECT_FALSE(ctx.has("y"));
}

TEST(CmdUnset, MissingArgument) {
    Context ctx;

    testing::internal::CaptureStdout();
    cmd_unset("", ctx, test_ui());
    string output = testing::internal::GetCapturedStdout();
    EXPECT_EQ(output, "Usage: unset <variable>\n");
}

TEST(CmdUnset, NonExistingVariable) {
    Context ctx;

    testing::internal::CaptureStdout();
    cmd_unset("1var", ctx, test_ui());
    string output = testing::internal::GetCapturedStdout();
    EXPECT_EQ(output, "Variable '1var' not found\n");
}

TEST(CmdSolve, SingleEquation) {
    Context ctx;
    ctx.set("a", 2);
    ctx.set("b", 3);

    testing::internal::CaptureStdout();
    cmd_solve("a * x + b = 11", ctx, test_ui());
    string output = testing::internal::GetCapturedStdout();
    EXPECT_EQ(output, "Solution:\n  x = 4\n");
}

TEST(CmdSolve, EmptyInput) {
    Context ctx;

    testing::internal::CaptureStdout();
    cmd_solve("", ctx, test_ui());
    string output = testing::internal::GetCapturedStdout();
    EXPECT_EQ(output, "Usage: solve <lhs> = <rhs>\n"
                      "       solve  (then enter equations, empty line to "
                      "solve)\n");
}

TEST(CmdSolve, InvalidEquation) {
    Context ctx;

    testing::internal::CaptureStdout();
    cmd_solve("2 ** x = 10", ctx, test_ui());
    string output = testing::internal::GetCapturedStdout();
    EXPECT_EQ(output, "Error: unexpected token '*'\n"
                      "  2 ** x = 10\n"
                      "     ^\n");
}

TEST(CmdSolve, NoSolution) {
    Context ctx;

    testing::internal::CaptureStdout();
    cmd_solve("0 * x + 5 = 0", ctx, test_ui());
    string output = testing::internal::GetCapturedStdout();
    EXPECT_EQ(output, "Error: equation has no solution (5.000000 != 0)\n"
                      "  0 * x + 5 = 0\n"
                      "  ^^^^^^^^^^^^^\n");
}

TEST(CmdSolve, InfiniteSolutions) {
    Context ctx;

    testing::internal::CaptureStdout();
    cmd_solve("0 * x = 0", ctx, test_ui());
    string output = testing::internal::GetCapturedStdout();
    EXPECT_EQ(output, "Error: equation is always true (0 = 0)\n"
                      "  0 * x = 0\n"
                      "  ^^^^^^^^^\n");
}

TEST(CmdSolve, MultipleUnknowns) {
    Context ctx;

    testing::internal::CaptureStdout();
    cmd_solve("x + y = 10", ctx, test_ui());
    string output = testing::internal::GetCapturedStdout();
    EXPECT_EQ(output, "Error: multiple unknowns in equation (x, y)\n"
                      "  x + y = 10\n"
                      "  ^^^^^^^^^^\n"
                      "Hint: use 'solve' alone for multi-variable systems, or "
                      "set to define variables\n");
}

TEST(CmdSolve, NonLinearEquation) {
    Context ctx;
    testing::internal::CaptureStdout();
    cmd_solve("a * x + b * y = c", ctx, test_ui());
    string output = testing::internal::GetCapturedStdout();
    EXPECT_EQ(output, "Error: non-linear term: multiplication of variables\n"
                      "  a * x + b * y = c\n"
                      "  ^^^^^\n");
}

TEST(CmdSolve, NonLinearEquationExponentiation) {
    Context ctx;
    testing::internal::CaptureStdout();
    cmd_solve("x^2 + y = 5", ctx, test_ui());
    string output = testing::internal::GetCapturedStdout();
    EXPECT_EQ(output, "Error: non-linear term: variable raised to power 2\n"
                      "  x^2 + y = 5\n"
                      "  ^^^\n");
}

TEST(CmdSolve, DivisionByVariable) {
    Context ctx;
    testing::internal::CaptureStdout();
    cmd_solve("x / y = 10", ctx, test_ui());
    string output = testing::internal::GetCapturedStdout();
    EXPECT_EQ(output, "Error: non-linear term: division by variable\n"
                      "  x / y = 10\n"
                      "  ^^^^^\n");
}

TEST(CmdSolve, DivisionByZero) {
    Context ctx;
    testing::internal::CaptureStdout();
    cmd_solve("x / 0 = 10", ctx, test_ui());
    string output = testing::internal::GetCapturedStdout();
    EXPECT_EQ(output, "Error: division by zero\n"
                      "  x / 0 = 10\n"
                      "      ^\n");
}

// ============================================================
// cmd_solve_system tests
// ============================================================

TEST(CmdSolveSystem, NoEquationsEntered) {
    Context ctx;

    testing::internal::CaptureStdout();
    istringstream input("\n");
    cmd_solve_system("", ctx, input, false, test_ui());
    string output = testing::internal::GetCapturedStdout();
    EXPECT_EQ(output, "No equations entered.\n");
}

TEST(CmdSolveSystem, ReturnsFalseWhenExpressionProvided) {
    Context       ctx;
    istringstream input("");
    bool result = cmd_solve_system("x + y = 5", ctx, input, false, test_ui());
    EXPECT_FALSE(result);
}

TEST(CmdSolveSystem, ReturnsTrueWhenNoExpression) {
    Context       ctx;
    istringstream input("\n");

    testing::internal::CaptureStdout();
    bool result = cmd_solve_system("", ctx, input, false, test_ui());
    testing::internal::GetCapturedStdout();

    EXPECT_TRUE(result);
}

TEST(CmdSolveSystem, TwoEquationsTwoUnknowns) {
    Context       ctx;
    // x + y = 10
    // x - y = 2
    // => x = 6, y = 4
    istringstream input("x + y = 10\nx - y = 2\n\n");

    testing::internal::CaptureStdout();
    bool   result = cmd_solve_system("", ctx, input, false, test_ui());
    string output = testing::internal::GetCapturedStdout();

    EXPECT_TRUE(result);
    EXPECT_TRUE(output.find("2 equation(s)") != string::npos);
    EXPECT_TRUE(output.find("2 variable(s)") != string::npos);
    EXPECT_TRUE(output.find("x = 6") != string::npos);
    EXPECT_TRUE(output.find("y = 4") != string::npos);
}

TEST(CmdSolveSystem, ThreeEquationsThreeUnknowns) {
    Context       ctx;
    // x + y + z = 6
    // x - y + z = 2
    // x + y - z = 0
    // => x = 1, y = 2, z = 3
    istringstream input("x + y + z = 6\nx - y + z = 2\nx + y - z = 0\n\n");

    testing::internal::CaptureStdout();
    bool   result = cmd_solve_system("", ctx, input, false, test_ui());
    string output = testing::internal::GetCapturedStdout();

    EXPECT_TRUE(result);
    EXPECT_TRUE(output.find("3 equation(s)") != string::npos);
    EXPECT_TRUE(output.find("3 variable(s)") != string::npos);
    EXPECT_TRUE(output.find("x = 1") != string::npos);
    EXPECT_TRUE(output.find("y = 2") != string::npos);
    EXPECT_TRUE(output.find("z = 3") != string::npos);
}

TEST(CmdSolveSystem, CancelAborts) {
    Context       ctx;
    istringstream input("x + y = 10\ncancel\n");

    testing::internal::CaptureStdout();
    bool   result = cmd_solve_system("", ctx, input, false, test_ui());
    string output = testing::internal::GetCapturedStdout();

    EXPECT_TRUE(result);
    EXPECT_TRUE(output.find("Cancelled.") != string::npos);
}

TEST(CmdSolveSystem, InteractiveModeShowsPrompts) {
    Context       ctx;
    istringstream input("x + y = 5\n2*x - y = 1\n\n");

    testing::internal::CaptureStdout();
    bool   result = cmd_solve_system("", ctx, input, true, test_ui());
    string output = testing::internal::GetCapturedStdout();

    EXPECT_TRUE(result);
    EXPECT_TRUE(output.find("Enter equations") != string::npos);
    EXPECT_TRUE(output.find("[1]") != string::npos);
    EXPECT_TRUE(output.find("[2]") != string::npos);
}

TEST(CmdSolveSystem, InteractiveCancelShowsPrompts) {
    Context       ctx;
    istringstream input("cancel\n");

    testing::internal::CaptureStdout();
    bool   result = cmd_solve_system("", ctx, input, true, test_ui());
    string output = testing::internal::GetCapturedStdout();

    EXPECT_TRUE(result);
    EXPECT_TRUE(output.find("Enter equations") != string::npos);
    EXPECT_TRUE(output.find("[1]") != string::npos);
    EXPECT_TRUE(output.find("Cancelled.") != string::npos);
}

TEST(CmdSolveSystem, WithContextSubstitution) {
    Context ctx;
    ctx.set("a", 2);
    ctx.set("b", 3);
    // a*x + y = 7  => 2x + y = 7
    // x + b*y = 8  => x + 3y = 8
    // => x = 2.6, y = 1.8  (or similar)
    // 2x + y = 7, x + 3y = 8 => x = (21-8)/5 = 13/5 = 2.6, y = 7-2*2.6 = 1.8
    istringstream input("a*x + y = 7\nx + b*y = 8\n\n");

    testing::internal::CaptureStdout();
    bool   result = cmd_solve_system("", ctx, input, false, test_ui());
    string output = testing::internal::GetCapturedStdout();

    EXPECT_TRUE(result);
    EXPECT_TRUE(output.find("2 equation(s)") != string::npos);
    EXPECT_TRUE(output.find("2 variable(s)") != string::npos);
    EXPECT_TRUE(output.find("x = 2.6") != string::npos);
    EXPECT_TRUE(output.find("y = 1.8") != string::npos);
}

TEST(CmdSolveSystem, InvalidEquationRetryThenSolve) {
    Context       ctx;
    // First line is invalid, second and third are valid
    istringstream input("2 ** x = 10\nx + y = 5\nx - y = 1\n\n");

    testing::internal::CaptureStdout();
    bool   result = cmd_solve_system("", ctx, input, false, test_ui());
    string output = testing::internal::GetCapturedStdout();

    EXPECT_TRUE(result);
    EXPECT_TRUE(output.find("Try again") != string::npos);
    EXPECT_TRUE(output.find("x = 3") != string::npos);
    EXPECT_TRUE(output.find("y = 2") != string::npos);
}

TEST(CmdSolveSystem, UnderDeterminedWarning) {
    Context       ctx;
    // 1 equation, 2 unknowns
    istringstream input("x + y = 5\n\n");

    testing::internal::CaptureStdout();
    bool   result = cmd_solve_system("", ctx, input, false, test_ui());
    string output = testing::internal::GetCapturedStdout();

    EXPECT_TRUE(result);
    EXPECT_TRUE(output.find("1 equation(s)") != string::npos);
    EXPECT_TRUE(output.find("2 variable(s)") != string::npos);
    EXPECT_TRUE(output.find("fewer equations than variables") != string::npos);
}

TEST(CmdSolveSystem, OverDeterminedWarning) {
    Context       ctx;
    // 3 equations, 1 unknown (but consistent)
    // x = 5, x = 5, x = 5
    istringstream input("x = 5\nx = 5\nx = 5\n\n");

    testing::internal::CaptureStdout();
    bool   result = cmd_solve_system("", ctx, input, false, test_ui());
    string output = testing::internal::GetCapturedStdout();

    EXPECT_TRUE(result);
    EXPECT_TRUE(output.find("3 equation(s)") != string::npos);
    EXPECT_TRUE(output.find("1 variable(s)") != string::npos);
    EXPECT_TRUE(output.find("more equations than variables") != string::npos);
}

TEST(CmdSolveSystem, WithExplicitVarsFlag) {
    Context       ctx;
    // Provide --vars to set variable ordering
    // y + x = 5
    // y - x = 1
    // => y = 3, x = 2
    istringstream input("y + x = 5\ny - x = 1\n\n");

    testing::internal::CaptureStdout();
    bool result = cmd_solve_system("--vars y x", ctx, input, false, test_ui());
    string output = testing::internal::GetCapturedStdout();

    EXPECT_TRUE(result);
    EXPECT_TRUE(output.find("y = 3") != string::npos);
    EXPECT_TRUE(output.find("x = 2") != string::npos);
}

TEST(CmdSolveSystem, WithFractionFlag) {
    Context       ctx;
    // x + 3*y = 10
    // 2*x + y = 5
    // => x = 1, y = 3
    // 2x + y = 5, x + 3y = 10 => x = (5-10)/(2-3)*...
    // x + 3y = 10, 2x + y = 5 => from first: x = 10-3y, sub: 20-6y+y=5 =>
    // -5y=-15 => y=3, x=1
    istringstream input("x + 3*y = 10\n2*x + y = 5\n\n");

    testing::internal::CaptureStdout();
    bool result = cmd_solve_system("--fraction", ctx, input, false, test_ui());
    string output = testing::internal::GetCapturedStdout();

    EXPECT_TRUE(result);
    EXPECT_TRUE(output.find("x = 1") != string::npos);
    EXPECT_TRUE(output.find("y = 3") != string::npos);
}

TEST(CmdSolveSystem, FractionFlagShowsFractions) {
    Context       ctx;
    // x + y = 1
    // x - y = 0
    // => x = 1/2, y = 1/2
    istringstream input("x + y = 1\nx - y = 0\n\n");

    testing::internal::CaptureStdout();
    bool result = cmd_solve_system("--fraction", ctx, input, false, test_ui());
    string output = testing::internal::GetCapturedStdout();

    EXPECT_TRUE(result);
    EXPECT_TRUE(output.find("1/2") != string::npos);
}

TEST(CmdSolveSystem, EOFWithoutEmptyLine) {
    Context       ctx;
    // Input ends (EOF) without an empty line separator
    istringstream input("x + y = 5\nx - y = 1");

    testing::internal::CaptureStdout();
    bool   result = cmd_solve_system("", ctx, input, false, test_ui());
    string output = testing::internal::GetCapturedStdout();

    EXPECT_TRUE(result);
    EXPECT_TRUE(output.find("2 equation(s)") != string::npos);
    EXPECT_TRUE(output.find("x = 3") != string::npos);
    EXPECT_TRUE(output.find("y = 2") != string::npos);
}

TEST(CmdSolveSystem, EOFWithNoInput) {
    Context       ctx;
    // Immediate EOF, no lines at all
    istringstream input("");

    testing::internal::CaptureStdout();
    bool   result = cmd_solve_system("", ctx, input, false, test_ui());
    string output = testing::internal::GetCapturedStdout();

    EXPECT_TRUE(result);
    EXPECT_EQ(output, "No equations entered.\n");
}

TEST(CmdSolveSystem, SingleEquationOneUnknown) {
    Context       ctx;
    // 2*x = 10 => x = 5
    istringstream input("2*x = 10\n\n");

    testing::internal::CaptureStdout();
    bool   result = cmd_solve_system("", ctx, input, false, test_ui());
    string output = testing::internal::GetCapturedStdout();

    EXPECT_TRUE(result);
    EXPECT_TRUE(output.find("1 equation(s)") != string::npos);
    EXPECT_TRUE(output.find("1 variable(s)") != string::npos);
    EXPECT_TRUE(output.find("x = 5") != string::npos);
}

TEST(CmdSolveSystem, InvalidThenCancel) {
    Context       ctx;
    istringstream input("2 ** x = 10\ncancel\n");

    testing::internal::CaptureStdout();
    bool   result = cmd_solve_system("", ctx, input, false, test_ui());
    string output = testing::internal::GetCapturedStdout();

    EXPECT_TRUE(result);
    EXPECT_TRUE(output.find("Try again") != string::npos);
    EXPECT_TRUE(output.find("Cancelled.") != string::npos);
}

TEST(CmdSolveSystem, MultipleInvalidThenValid) {
    Context       ctx;
    // Two invalid equations, then two valid ones
    istringstream input("bad eq\nalso bad!!\nx + y = 7\nx - y = 3\n\n");

    testing::internal::CaptureStdout();
    bool   result = cmd_solve_system("", ctx, input, false, test_ui());
    string output = testing::internal::GetCapturedStdout();

    EXPECT_TRUE(result);
    EXPECT_TRUE(output.find("x = 5") != string::npos);
    EXPECT_TRUE(output.find("y = 2") != string::npos);
}

TEST(CmdSolveSystem, NegativeCoefficients) {
    Context       ctx;
    // -x + 2*y = 1
    // 3*x - y = 11
    // From first: x = 2y - 1, sub: 3(2y-1) - y = 11 => 6y-3-y=11 => 5y=14 =>
    // y=14/5=2.8, x=4.6
    istringstream input("-x + 2*y = 1\n3*x - y = 11\n\n");

    testing::internal::CaptureStdout();
    bool   result = cmd_solve_system("", ctx, input, false, test_ui());
    string output = testing::internal::GetCapturedStdout();

    EXPECT_TRUE(result);
    EXPECT_TRUE(output.find("2 equation(s)") != string::npos);
    EXPECT_TRUE(output.find("2 variable(s)") != string::npos);
}

TEST(CmdSolveSystem, EquationNumberIncrementsOnlyOnValid) {
    Context       ctx;
    // Invalid eq should not increment eq_num
    // Valid eq 1, invalid, valid eq 2
    istringstream input("x + y = 10\n2 ** z = 5\nx - y = 2\n\n");

    testing::internal::CaptureStdout();
    bool   result = cmd_solve_system("", ctx, input, true, test_ui());
    string output = testing::internal::GetCapturedStdout();

    EXPECT_TRUE(result);
    // Should see [1], [2] for first valid, then invalid prompt, then [2] again
    // for retry
    EXPECT_TRUE(output.find("[1]") != string::npos);
    EXPECT_TRUE(output.find("[2]") != string::npos);
    // The invalid line stays at same eq number
    EXPECT_TRUE(output.find("Try again") != string::npos);
    EXPECT_TRUE(output.find("x = 6") != string::npos);
    EXPECT_TRUE(output.find("y = 4") != string::npos);
}

TEST(CmdSolveSystem, VariablesSortedAlphabetically) {
    Context       ctx;
    // z + b = 5
    // z - b = 1
    // => b = 2, z = 3  (alphabetical: b before z)
    istringstream input("z + b = 5\nz - b = 1\n\n");

    testing::internal::CaptureStdout();
    bool   result = cmd_solve_system("", ctx, input, false, test_ui());
    string output = testing::internal::GetCapturedStdout();

    EXPECT_TRUE(result);
    EXPECT_TRUE(output.find("b = 2") != string::npos);
    EXPECT_TRUE(output.find("z = 3") != string::npos);
}

// ============================================================
// cmd_clear tests
// ============================================================

// ทดสอบ clear เมื่อมีตัวแปรอยู่ - ต้องลบหมดและแสดงจำนวนที่ลบ
TEST(CmdClear, ClearsExistingVariables) {
    Context ctx;
    ctx.set("x", 1);
    ctx.set("y", 2);
    ctx.set("z", 3);

    testing::internal::CaptureStdout();
    cmd_clear(ctx, test_ui());
    string output = testing::internal::GetCapturedStdout();

    EXPECT_TRUE(ctx.empty());
    EXPECT_TRUE(output.find("Cleared 3 variable(s)") != string::npos);
}

// ทดสอบ clear เมื่อไม่มีตัวแปร - ต้องแสดง 0
TEST(CmdClear, ClearsEmptyContext) {
    Context ctx;

    testing::internal::CaptureStdout();
    cmd_clear(ctx, test_ui());
    string output = testing::internal::GetCapturedStdout();

    EXPECT_EQ(output, "Cleared 0 variable(s)\n");
}

// ============================================================
// cmd_vars tests
// ============================================================

// ทดสอบ vars เมื่อไม่มีตัวแปร - ต้องแสดง "No variables defined"
TEST(CmdVars, EmptyContext) {
    Context ctx;

    testing::internal::CaptureStdout();
    cmd_vars(ctx, test_ui());
    string output = testing::internal::GetCapturedStdout();

    EXPECT_EQ(output, "No variables defined.\n");
}

// ทดสอบ vars เมื่อมีตัวแปรอยู่ - ต้องแสดงรายชื่อตัวแปรพร้อมค่า
TEST(CmdVars, ShowsDefinedVariables) {
    Context ctx;
    ctx.set("x", 42);
    ctx.set("pi", 3.14);

    testing::internal::CaptureStdout();
    cmd_vars(ctx, test_ui());
    string output = testing::internal::GetCapturedStdout();

    EXPECT_TRUE(output.find("Variables:") != string::npos);
    EXPECT_TRUE(output.find("x = 42") != string::npos);
    EXPECT_TRUE(output.find("pi = 3.14") != string::npos);
}

// ============================================================
// cmd_set edge case tests
// ============================================================

// ทดสอบ set ด้วยชื่อตัวแปรที่มี underscore (ต้องใช้ได้)
TEST(CmdSet, UnderscoreVariableName) {
    Context ctx;
    cmd_set("my_var 10", ctx, test_ui());

    EXPECT_TRUE(ctx.has("my_var"));
    EXPECT_DOUBLE_EQ(ctx.get("my_var"), 10.0);
}

// ทดสอบ set ด้วย expression เป็นค่า (เช่น set x 2+3 => x=5)
TEST(CmdSet, ExpressionValue) {
    Context ctx;

    testing::internal::CaptureStdout();
    cmd_set("x 2+3", ctx, test_ui());
    string output = testing::internal::GetCapturedStdout();

    EXPECT_TRUE(ctx.has("x"));
    EXPECT_DOUBLE_EQ(ctx.get("x"), 5.0);
    EXPECT_TRUE(output.find("x = 5") != string::npos);
}

// ทดสอบ set ค่าลบ (set x -7)
TEST(CmdSet, NegativeValue) {
    Context ctx;

    testing::internal::CaptureStdout();
    cmd_set("x -7", ctx, test_ui());
    string output = testing::internal::GetCapturedStdout();

    EXPECT_TRUE(ctx.has("x"));
    EXPECT_DOUBLE_EQ(ctx.get("x"), -7.0);
    EXPECT_TRUE(output.find("x = -7") != string::npos);
}

// ทดสอบ set ทับค่าเดิม (overwrite)
TEST(CmdSet, OverwriteExistingVariable) {
    Context ctx;
    ctx.set("x", 1);

    testing::internal::CaptureStdout();
    cmd_set("x 99", ctx, test_ui());
    testing::internal::GetCapturedStdout();

    EXPECT_DOUBLE_EQ(ctx.get("x"), 99.0);
}

// ============================================================
// cmd_simplify tests
// ============================================================

// ทดสอบ simplify เมื่อไม่มี argument - ต้องแสดง usage
TEST(CmdSimplify, EmptyInput) {
    Context ctx;

    testing::internal::CaptureStdout();
    cmd_simplify("", ctx, test_ui());
    string output = testing::internal::GetCapturedStdout();

    EXPECT_TRUE(output.find("Usage: simplify") != string::npos);
}

// ทดสอบ simplify เมื่อมีแต่ flags ไม่มี expression - ต้องแสดง usage
TEST(CmdSimplify, FlagsOnlyNoExpression) {
    Context ctx;

    testing::internal::CaptureStdout();
    cmd_simplify("--fraction", ctx, test_ui());
    string output = testing::internal::GetCapturedStdout();

    EXPECT_TRUE(output.find("Usage: simplify") != string::npos);
}

// ทดสอบ simplify สมการพื้นฐาน (2x + 3x = 10 => 5x = 10)
TEST(CmdSimplify, BasicEquation) {
    Context ctx;

    testing::internal::CaptureStdout();
    cmd_simplify("2*x + 3*x = 10", ctx, test_ui());
    string output = testing::internal::GetCapturedStdout();

    EXPECT_TRUE(output.find("x = 2") != string::npos);
}

// ทดสอบ simplify กับ --fraction flag
TEST(CmdSimplify, WithFractionFlag) {
    Context ctx;

    testing::internal::CaptureStdout();
    cmd_simplify("x + y = 3 --fraction", ctx, test_ui());
    string output = testing::internal::GetCapturedStdout();

    // ต้องแสดงผลออกมาได้ (ไม่ error)
    EXPECT_TRUE(output.find("=") != string::npos);
}

// ทดสอบ simplify กับ parse error
TEST(CmdSimplify, InvalidExpression) {
    Context ctx;

    testing::internal::CaptureStdout();
    cmd_simplify("2 ** x = 5", ctx, test_ui());
    string output = testing::internal::GetCapturedStdout();

    EXPECT_TRUE(output.find("Error:") != string::npos);
}

// ============================================================
// cmd_evaluate tests
// ============================================================

// ทดสอบ evaluate expression ง่ายๆ (เช่น 2 + 3)
TEST(CmdEvaluate, SimpleExpression) {
    Context ctx;

    testing::internal::CaptureStdout();
    cmd_evaluate("2 + 3", ctx, test_ui());
    string output = testing::internal::GetCapturedStdout();

    EXPECT_EQ(output, "5\n");
}

// ทดสอบ evaluate expression ที่มีตัวแปร (ต้อง set ไว้ก่อน)
TEST(CmdEvaluate, ExpressionWithVariable) {
    Context ctx;
    ctx.set("x", 10);

    testing::internal::CaptureStdout();
    cmd_evaluate("x * 2 + 1", ctx, test_ui());
    string output = testing::internal::GetCapturedStdout();

    EXPECT_EQ(output, "21\n");
}

// ทดสอบ evaluate equation ที่เป็นจริง (เช่น 5 = 5)
TEST(CmdEvaluate, TrueEquation) {
    Context ctx;

    testing::internal::CaptureStdout();
    cmd_evaluate("2 + 3 = 5", ctx, test_ui());
    string output = testing::internal::GetCapturedStdout();

    EXPECT_TRUE(output.find("true") != string::npos);
    EXPECT_TRUE(output.find("5 = 5") != string::npos);
}

// ทดสอบ evaluate equation ที่เป็นเท็จ (เช่น 2 + 3 = 6)
TEST(CmdEvaluate, FalseEquation) {
    Context ctx;

    testing::internal::CaptureStdout();
    cmd_evaluate("2 + 3 = 6", ctx, test_ui());
    string output = testing::internal::GetCapturedStdout();

    EXPECT_TRUE(output.find("false") != string::npos);
}

// ทดสอบ evaluate expression ที่มี undefined variable
TEST(CmdEvaluate, UndefinedVariable) {
    Context ctx;

    testing::internal::CaptureStdout();
    cmd_evaluate("x + 1", ctx, test_ui());
    string output = testing::internal::GetCapturedStdout();

    EXPECT_TRUE(output.find("Error:") != string::npos);
    EXPECT_TRUE(output.find("undefined variable") != string::npos);
}

// ทดสอบ evaluate กับ parse error
TEST(CmdEvaluate, ParseError) {
    Context ctx;

    testing::internal::CaptureStdout();
    cmd_evaluate("2 ** 3", ctx, test_ui());
    string output = testing::internal::GetCapturedStdout();

    EXPECT_TRUE(output.find("Error:") != string::npos);
}

// ============================================================
// process_input_line tests
// ============================================================

// ทดสอบ empty input - ต้อง return true (ไม่ exit)
TEST(ProcessInputLine, EmptyInput) {
    Context       ctx;
    istringstream in("");
    bool          result = process_input_line("", ctx, in, false, test_ui());
    EXPECT_TRUE(result);
}

// ทดสอบ "exit" command - ต้อง return false
TEST(ProcessInputLine, ExitCommand) {
    Context       ctx;
    istringstream in("");
    bool result = process_input_line("exit", ctx, in, false, test_ui());
    EXPECT_FALSE(result);
}

// ทดสอบ "q" command - ต้อง return false เช่นกัน
TEST(ProcessInputLine, QuitShortcut) {
    Context       ctx;
    istringstream in("");
    bool          result = process_input_line("q", ctx, in, false, test_ui());
    EXPECT_FALSE(result);
}

// ทดสอบ "help" command - ต้อง return true และแสดง help text
TEST(ProcessInputLine, HelpCommand) {
    Context       ctx;
    istringstream in("");

    testing::internal::CaptureStdout();
    bool   result = process_input_line("help", ctx, in, false, test_ui());
    string output = testing::internal::GetCapturedStdout();

    EXPECT_TRUE(result);
    EXPECT_TRUE(output.find("Math Solver") != string::npos);
}

// ทดสอบ "set" routing - ต้องส่งไปที่ cmd_set
TEST(ProcessInputLine, SetRouting) {
    Context       ctx;
    istringstream in("");

    testing::internal::CaptureStdout();
    process_input_line("set x 5", ctx, in, false, test_ui());
    testing::internal::GetCapturedStdout();

    EXPECT_TRUE(ctx.has("x"));
    EXPECT_DOUBLE_EQ(ctx.get("x"), 5.0);
}

// ทดสอบ "unset" routing
TEST(ProcessInputLine, UnsetRouting) {
    Context ctx;
    ctx.set("x", 5);
    istringstream in("");

    testing::internal::CaptureStdout();
    process_input_line("unset x", ctx, in, false, test_ui());
    testing::internal::GetCapturedStdout();

    EXPECT_FALSE(ctx.has("x"));
}

// ทดสอบ "clear" routing
TEST(ProcessInputLine, ClearRouting) {
    Context ctx;
    ctx.set("x", 1);
    ctx.set("y", 2);
    istringstream in("");

    testing::internal::CaptureStdout();
    process_input_line("clear", ctx, in, false, test_ui());
    testing::internal::GetCapturedStdout();

    EXPECT_TRUE(ctx.empty());
}

// ทดสอบ "vars" routing
TEST(ProcessInputLine, VarsRouting) {
    Context       ctx;
    istringstream in("");

    testing::internal::CaptureStdout();
    process_input_line("vars", ctx, in, false, test_ui());
    string output = testing::internal::GetCapturedStdout();

    EXPECT_TRUE(output.find("No variables defined") != string::npos);
}

// ทดสอบ "solve <equation>" routing (single equation)
TEST(ProcessInputLine, SolveSingleRouting) {
    Context       ctx;
    istringstream in("");

    testing::internal::CaptureStdout();
    process_input_line("solve 2*x = 10", ctx, in, false, test_ui());
    string output = testing::internal::GetCapturedStdout();

    EXPECT_TRUE(output.find("x = 5") != string::npos);
}

// ทดสอบ "simplify <equation>" routing
TEST(ProcessInputLine, SimplifyRouting) {
    Context       ctx;
    istringstream in("");

    testing::internal::CaptureStdout();
    process_input_line("simplify x + x = 4", ctx, in, false, test_ui());
    string output = testing::internal::GetCapturedStdout();

    EXPECT_TRUE(output.find("x = 2") != string::npos);
}

// ทดสอบ default expression evaluation routing
TEST(ProcessInputLine, DefaultExpressionRouting) {
    Context       ctx;
    istringstream in("");

    testing::internal::CaptureStdout();
    process_input_line("2 + 3 * 4", ctx, in, false, test_ui());
    string output = testing::internal::GetCapturedStdout();

    EXPECT_EQ(output, "14\n");
}

// ทดสอบ whitespace trimming ใน process_input_line
TEST(ProcessInputLine, WhitespaceTrimming) {
    Context       ctx;
    istringstream in("");

    testing::internal::CaptureStdout();
    process_input_line("   2 + 3   ", ctx, in, false, test_ui());
    string output = testing::internal::GetCapturedStdout();

    EXPECT_EQ(output, "5\n");
}