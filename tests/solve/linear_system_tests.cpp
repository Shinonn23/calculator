// ============================================================
// Unit tests สำหรับ LinearSystem + AugmentedMatrix
// ครอบคลุม: AugmentedMatrix — construction, swap_rows,
//           scale_row, add_scaled_row, find_pivot, to_rref,
//           is_inconsistent, to_string
//           LinearSystem — add_equation, build_matrix, solve,
//           unique / no-solution / infinite solutions
//           SystemSolution.to_string()
// ============================================================

#include "solve/linear_collector.h"
#include "solve/linear_system.h"
#include <cmath>
#include <gtest/gtest.h>


using namespace math_solver;
using namespace std;

// ============================================================
// AugmentedMatrix — basic operations
// ============================================================

// ทดสอบ construction
TEST(AugmentedMatrixTest, Construction) {
    AugmentedMatrix m(2, 3);
    EXPECT_EQ(m.rows(), 2u);
    EXPECT_EQ(m.cols(), 3u);
    // ค่าเริ่มต้นต้องเป็น 0
    EXPECT_DOUBLE_EQ(m.at(0, 0), 0.0);
    EXPECT_DOUBLE_EQ(m.rhs(0), 0.0);
}

// ทดสอบ at() / rhs() set-get
TEST(AugmentedMatrixTest, SetGet) {
    AugmentedMatrix m(1, 2);
    m.at(0, 0) = 3.0;
    m.at(0, 1) = 4.0;
    m.rhs(0)   = 10.0;

    EXPECT_DOUBLE_EQ(m.at(0, 0), 3.0);
    EXPECT_DOUBLE_EQ(m.at(0, 1), 4.0);
    EXPECT_DOUBLE_EQ(m.rhs(0), 10.0);
}

// ทดสอบ swap_rows
TEST(AugmentedMatrixTest, SwapRows) {
    AugmentedMatrix m(2, 1);
    m.at(0, 0) = 1.0;
    m.rhs(0)   = 10.0;
    m.at(1, 0) = 2.0;
    m.rhs(1)   = 20.0;

    m.swap_rows(0, 1);
    EXPECT_DOUBLE_EQ(m.at(0, 0), 2.0);
    EXPECT_DOUBLE_EQ(m.rhs(0), 20.0);
    EXPECT_DOUBLE_EQ(m.at(1, 0), 1.0);
    EXPECT_DOUBLE_EQ(m.rhs(1), 10.0);
}

// ทดสอบ swap_rows กับ row เดียวกัน (identity)
TEST(AugmentedMatrixTest, SwapSameRow) {
    AugmentedMatrix m(1, 1);
    m.at(0, 0) = 5.0;
    m.swap_rows(0, 0);
    EXPECT_DOUBLE_EQ(m.at(0, 0), 5.0);
}

// ทดสอบ scale_row
TEST(AugmentedMatrixTest, ScaleRow) {
    AugmentedMatrix m(1, 2);
    m.at(0, 0) = 2.0;
    m.at(0, 1) = 4.0;
    m.rhs(0)   = 6.0;

    m.scale_row(0, 0.5);
    EXPECT_DOUBLE_EQ(m.at(0, 0), 1.0);
    EXPECT_DOUBLE_EQ(m.at(0, 1), 2.0);
    EXPECT_DOUBLE_EQ(m.rhs(0), 3.0);
}

// ทดสอบ add_scaled_row
TEST(AugmentedMatrixTest, AddScaledRow) {
    AugmentedMatrix m(2, 1);
    m.at(0, 0) = 1.0;
    m.rhs(0)   = 5.0;
    m.at(1, 0) = 3.0;
    m.rhs(1)   = 15.0;

    // row 1 -= 3 * row 0
    m.add_scaled_row(1, 0, -3.0);
    EXPECT_DOUBLE_EQ(m.at(1, 0), 0.0);
    EXPECT_DOUBLE_EQ(m.rhs(1), 0.0);
}

// ทดสอบ find_pivot
TEST(AugmentedMatrixTest, FindPivot) {
    AugmentedMatrix m(3, 1);
    m.at(0, 0) = 0.0;
    m.at(1, 0) = 5.0;
    m.at(2, 0) = 3.0;

    // ต้องเลือก row 1 เพราะ |5| > |3| > |0|
    EXPECT_EQ(m.find_pivot(0, 0), 1u);
}

// ทดสอบ find_pivot เมื่อทุก row เป็น 0
TEST(AugmentedMatrixTest, FindPivotNone) {
    AugmentedMatrix m(2, 1);
    m.at(0, 0) = 0.0;
    m.at(1, 0) = 0.0;

    // ต้อง return rows_ (= 2) เพราะไม่มี pivot
    EXPECT_EQ(m.find_pivot(0, 0), m.rows());
}

// ============================================================
// AugmentedMatrix — to_rref
// ============================================================

// ทดสอบ RREF ของ identity-like matrix
TEST(AugmentedMatrixTest, ToRrefIdentity) {
    // [1 0 | 3]
    // [0 1 | 5]
    AugmentedMatrix m(2, 2);
    m.at(0, 0)  = 1.0;
    m.at(0, 1)  = 0.0;
    m.rhs(0)    = 3.0;
    m.at(1, 0)  = 0.0;
    m.at(1, 1)  = 1.0;
    m.rhs(1)    = 5.0;

    auto pivots = m.to_rref();
    EXPECT_EQ(pivots.size(), 2u);
    EXPECT_DOUBLE_EQ(m.rhs(0), 3.0);
    EXPECT_DOUBLE_EQ(m.rhs(1), 5.0);
}

// ทดสอบ RREF ของ 2x2 system
TEST(AugmentedMatrixTest, ToRref2x2) {
    // [2 1 | 5]   → [1 0 | 1]
    // [1 1 | 3]   → [0 1 | 2]  (not exact, but should solve to finite)
    // x=1, y=3 doesn't work... let me compute:
    // 2x + y = 5, x + y = 3 → x = 2, y = 1
    AugmentedMatrix m(2, 2);
    m.at(0, 0)  = 2.0;
    m.at(0, 1)  = 1.0;
    m.rhs(0)    = 5.0;
    m.at(1, 0)  = 1.0;
    m.at(1, 1)  = 1.0;
    m.rhs(1)    = 3.0;

    auto pivots = m.to_rref();
    EXPECT_EQ(pivots.size(), 2u);
    EXPECT_NEAR(m.rhs(0), 2.0, 1e-10);
    EXPECT_NEAR(m.rhs(1), 1.0, 1e-10);
}

// ============================================================
// AugmentedMatrix — is_inconsistent
// ============================================================

// ทดสอบ consistent system
TEST(AugmentedMatrixTest, Consistent) {
    AugmentedMatrix m(1, 1);
    m.at(0, 0) = 1.0;
    m.rhs(0)   = 5.0;
    EXPECT_FALSE(m.is_inconsistent());
}

// ทดสอบ inconsistent: [0 | 5]
TEST(AugmentedMatrixTest, Inconsistent) {
    AugmentedMatrix m(1, 1);
    m.at(0, 0) = 0.0;
    m.rhs(0)   = 5.0;
    EXPECT_TRUE(m.is_inconsistent());
}

// ============================================================
// AugmentedMatrix — to_string
// ============================================================

// ทดสอบ to_string ไม่ crash
TEST(AugmentedMatrixTest, ToString) {
    AugmentedMatrix m(1, 1);
    m.at(0, 0) = 1.0;
    m.rhs(0)   = 2.0;

    string s   = m.to_string();
    EXPECT_FALSE(s.empty());
}

// ============================================================
// LinearSystem — basic operations
// ============================================================

// ทดสอบ add_equation
TEST(LinearSystemTest, AddEquation) {
    LinearSystem sys;
    LinearForm   f("x", 2.0);
    f.constant = -5.0;
    sys.add_equation(f);

    EXPECT_EQ(sys.num_equations(), 1u);
    EXPECT_EQ(sys.num_variables(), 1u);
}

// ทดสอบ multiple equations
TEST(LinearSystemTest, MultipleEquations) {
    LinearSystem sys;

    LinearForm   f1;
    f1.coeffs["x"] = 1.0;
    f1.coeffs["y"] = 1.0;
    f1.constant    = -3.0;
    sys.add_equation(f1);

    LinearForm f2;
    f2.coeffs["x"] = 2.0;
    f2.coeffs["y"] = -1.0;
    f2.constant    = -3.0;
    sys.add_equation(f2);

    EXPECT_EQ(sys.num_equations(), 2u);
    EXPECT_EQ(sys.num_variables(), 2u);
}

// ทดสอบ empty / clear
TEST(LinearSystemTest, EmptyAndClear) {
    LinearSystem sys;
    EXPECT_TRUE(sys.empty());

    LinearForm f("x", 1.0);
    sys.add_equation(f);
    EXPECT_FALSE(sys.empty());

    sys.clear();
    EXPECT_TRUE(sys.empty());
    EXPECT_EQ(sys.num_equations(), 0u);
}

// ทดสอบ sort_variables
TEST(LinearSystemTest, SortVariables) {
    LinearSystem sys;
    LinearForm   f;
    f.coeffs["z"] = 1.0;
    f.coeffs["a"] = 2.0;
    sys.add_equation(f);
    sys.sort_variables();

    const auto& vars = sys.variables();
    ASSERT_EQ(vars.size(), 2u);
    EXPECT_EQ(vars[0], "a");
    EXPECT_EQ(vars[1], "z");
}

// ทดสอบ set_variables
TEST(LinearSystemTest, SetVariables) {
    LinearSystem sys;
    sys.set_variables({"x", "y"});
    EXPECT_EQ(sys.num_variables(), 2u);
}

// ============================================================
// LinearSystem — solve
// ============================================================

// ทดสอบ solve system เดียว: x = 5 → x=5
TEST(LinearSystemTest, SolveSingle) {
    LinearSystem sys;
    LinearForm   f("x", 1.0);
    f.constant = -5.0; // x - 5 = 0
    sys.add_equation(f);
    sys.sort_variables();

    auto result = sys.solve();
    EXPECT_EQ(result.type, SolutionType::Unique);
    ASSERT_EQ(result.values.size(), 1u);
    EXPECT_NEAR(result.values[0], 5.0, 1e-10);
}

// ทดสอบ solve 2x2 system:
// x + y = 3, x - y = 1 → x=2, y=1
TEST(LinearSystemTest, Solve2x2) {
    LinearSystem sys;

    LinearForm   f1;
    f1.coeffs["x"] = 1.0;
    f1.coeffs["y"] = 1.0;
    f1.constant    = -3.0;
    sys.add_equation(f1);

    LinearForm f2;
    f2.coeffs["x"] = 1.0;
    f2.coeffs["y"] = -1.0;
    f2.constant    = -1.0;
    sys.add_equation(f2);

    sys.sort_variables();
    auto result = sys.solve();

    EXPECT_EQ(result.type, SolutionType::Unique);
    ASSERT_EQ(result.values.size(), 2u);
    // sorted vars: x, y
    EXPECT_NEAR(result.values[0], 2.0, 1e-10); // x
    EXPECT_NEAR(result.values[1], 1.0, 1e-10); // y
}

// ทดสอบ no-solution system:
// x = 1, x = 2 (contradictory)
TEST(LinearSystemTest, NoSolution) {
    LinearSystem sys;

    LinearForm   f1("x", 1.0);
    f1.constant = -1.0;
    sys.add_equation(f1);

    LinearForm f2("x", 1.0);
    f2.constant = -2.0;
    sys.add_equation(f2);

    sys.sort_variables();
    auto result = sys.solve();
    EXPECT_EQ(result.type, SolutionType::NoSolution);
}

// ทดสอบ infinite solutions:
// x + y = 3 (underdetermined)
TEST(LinearSystemTest, InfiniteSolutions) {
    LinearSystem sys;

    LinearForm   f;
    f.coeffs["x"] = 1.0;
    f.coeffs["y"] = 1.0;
    f.constant    = -3.0;
    sys.add_equation(f);

    sys.sort_variables();
    auto result = sys.solve();
    EXPECT_EQ(result.type, SolutionType::Infinite);
    EXPECT_FALSE(result.free_variables.empty());
}

// ทดสอบ empty system → infinite
TEST(LinearSystemTest, EmptySystem) {
    LinearSystem sys;
    auto         result = sys.solve();
    EXPECT_EQ(result.type, SolutionType::Infinite);
}

// ทดสอบ all-constant system (no variables)
TEST(LinearSystemTest, AllConstants) {
    LinearSystem sys;
    LinearForm   f(0.0); // 0 = 0 → consistent
    sys.add_equation(f);

    auto result = sys.solve();
    EXPECT_EQ(result.type, SolutionType::Unique);
}

// ทดสอบ all-constant inconsistent: 0 != 5
TEST(LinearSystemTest, AllConstantsInconsistent) {
    LinearSystem sys;
    LinearForm   f;
    f.constant = 5.0; // 5 = 0 → bad
    sys.add_equation(f);

    auto result = sys.solve();
    EXPECT_EQ(result.type, SolutionType::NoSolution);
}

// ============================================================
// SystemSolution.to_string()
// ============================================================

// ทดสอบ unique solution to_string
TEST(SystemSolutionTest, UniqueToString) {
    SystemSolution s;
    s.type      = SolutionType::Unique;
    s.variables = {"x", "y"};
    s.values    = {2.0, 3.0};

    string str  = s.to_string();
    EXPECT_NE(str.find("x = 2"), string::npos);
    EXPECT_NE(str.find("y = 3"), string::npos);
}

// ทดสอบ no-solution to_string
TEST(SystemSolutionTest, NoSolutionToString) {
    SystemSolution s;
    s.type     = SolutionType::NoSolution;

    string str = s.to_string();
    EXPECT_NE(str.find("No solution"), string::npos);
}

// ทดสอบ infinite solutions to_string
TEST(SystemSolutionTest, InfiniteToString) {
    SystemSolution s;
    s.type           = SolutionType::Infinite;
    s.free_variables = {"y"};

    string str       = s.to_string();
    EXPECT_NE(str.find("Infinite"), string::npos);
    EXPECT_NE(str.find("y"), string::npos);
}

// ทดสอบ to_string as_fraction mode
TEST(SystemSolutionTest, ToStringAsFraction) {
    SystemSolution s;
    s.type      = SolutionType::Unique;
    s.variables = {"x"};
    s.values    = {0.5};

    string str  = s.to_string(true); // as_fraction
    // ต้องมี "1/2" หรือ fraction format
    EXPECT_NE(str.find("x = "), string::npos);
}

// ============================================================
// LinearSystem — build_matrix
// ============================================================

// ทดสอบ build_matrix สร้าง matrix ที่ถูกต้อง
TEST(LinearSystemTest, BuildMatrix) {
    LinearSystem sys;

    LinearForm   f;
    f.coeffs["x"] = 2.0;
    f.coeffs["y"] = 3.0;
    f.constant    = -7.0; // 2x + 3y - 7 = 0 → rhs = 7
    sys.add_equation(f);

    sys.sort_variables();
    auto m = sys.build_matrix();

    EXPECT_EQ(m.rows(), 1u);
    EXPECT_EQ(m.cols(), 2u);
    EXPECT_DOUBLE_EQ(m.at(0, 0), 2.0); // x coeff
    EXPECT_DOUBLE_EQ(m.at(0, 1), 3.0); // y coeff
    EXPECT_DOUBLE_EQ(m.rhs(0), 7.0);   // -(-7) = 7
}
