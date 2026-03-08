#include <gtest/gtest.h>

#include "algebra/linear/linear_collector.hpp"
#include "algebra/matrix/matrix_solver.hpp"
#include "algebra/matrix/solve_method.hpp"

using namespace math_solver;

// ─── helpers ────────────────────────────────────────────────────────────────

// Build a LinearForm from explicit coefficients and a constant.
// Example: make_form({{"x", 2.0}, {"y", 1.0}}, -3.0) → 2x + y = 3
static LinearForm make_form(std::map<std::string, double> coeffs, double constant) {
    LinearForm f;
    f.coeffs = std::move(coeffs);
    f.constant = constant;
    return f;
}

// ─── unique solution (Gauss) ─────────────────────────────────────────────────

TEST(MatrixSolver, Simple2x2UniqueGauss) {
    // x + y = 3
    // x - y = 1
    // → x = 2, y = 1
    std::vector<LinearForm> forms = {
        make_form({{"x", 1.0}, {"y", 1.0}}, -3.0),
        make_form({{"x", 1.0}, {"y", -1.0}}, -1.0),
    };
    std::vector<std::string> vars = {"x", "y"};
    MatrixSolver solver;
    auto r = solver.solve(forms, vars);
    ASSERT_TRUE(r.ok());
    EXPECT_NEAR((*r).solutions["x"], 2.0, 1e-9);
    EXPECT_NEAR((*r).solutions["y"], 1.0, 1e-9);
    EXPECT_TRUE((*r).is_unique);
    EXPECT_EQ((*r).rank_A, 2);
    EXPECT_EQ((*r).rank_Ab, 2);
}

TEST(MatrixSolver, Simple3x3Gauss) {
    // x + y + z = 6
    // x + 2y + z = 8
    // x + y + 2z = 9
    // → x = 1, y = 2, z = 3
    std::vector<LinearForm> forms = {
        make_form({{"x", 1.0}, {"y", 1.0}, {"z", 1.0}}, -6.0),
        make_form({{"x", 1.0}, {"y", 2.0}, {"z", 1.0}}, -8.0),
        make_form({{"x", 1.0}, {"y", 1.0}, {"z", 2.0}}, -9.0),
    };
    std::vector<std::string> vars = {"x", "y", "z"};
    MatrixSolver solver;
    auto r = solver.solve(forms, vars);
    ASSERT_TRUE(r.ok());
    EXPECT_NEAR((*r).solutions["x"], 1.0, 1e-9);
    EXPECT_NEAR((*r).solutions["y"], 2.0, 1e-9);
    EXPECT_NEAR((*r).solutions["z"], 3.0, 1e-9);
}

// ─── unique solution (LU) ────────────────────────────────────────────────────

TEST(MatrixSolver, Simple2x2UniqueLU) {
    std::vector<LinearForm> forms = {
        make_form({{"x", 1.0}, {"y", 1.0}}, -3.0),
        make_form({{"x", 1.0}, {"y", -1.0}}, -1.0),
    };
    std::vector<std::string> vars = {"x", "y"};
    MatrixSolver solver;
    SolveSystemOptions opts;
    opts.method = SolveMethod::LU;
    auto r = solver.solve(forms, vars, opts);
    ASSERT_TRUE(r.ok());
    EXPECT_NEAR((*r).solutions["x"], 2.0, 1e-9);
    EXPECT_NEAR((*r).solutions["y"], 1.0, 1e-9);
}

// ─── inconsistent system ─────────────────────────────────────────────────────

TEST(MatrixSolver, InconsistentSystem_NoSolution) {
    // x + y = 1
    // x + y = 2
    // → inconsistent
    std::vector<LinearForm> forms = {
        make_form({{"x", 1.0}, {"y", 1.0}}, -1.0),
        make_form({{"x", 1.0}, {"y", 1.0}}, -2.0),
    };
    std::vector<std::string> vars = {"x", "y"};
    MatrixSolver solver;
    auto r = solver.solve(forms, vars);
    EXPECT_FALSE(r.ok());
}

// ─── underdetermined / infinite solutions ────────────────────────────────────

TEST(MatrixSolver, Underdetermined_InfiniteSolutions) {
    // x + y = 5 — one equation, two unknowns
    std::vector<LinearForm> forms = {
        make_form({{"x", 1.0}, {"y", 1.0}}, -5.0),
    };
    std::vector<std::string> vars = {"x", "y"};
    MatrixSolver solver;
    auto r = solver.solve(forms, vars);
    EXPECT_FALSE(r.ok());
}

TEST(MatrixSolver, Underdetermined_FreeVarsMode) {
    // x + y = 5 — free-variable mode should succeed
    std::vector<LinearForm> forms = {
        make_form({{"x", 1.0}, {"y", 1.0}}, -5.0),
    };
    std::vector<std::string> vars = {"x", "y"};
    MatrixSolver solver;
    SolveSystemOptions opts;
    opts.free_vars = true;
    auto r = solver.solve(forms, vars, opts);
    ASSERT_TRUE(r.ok());
    EXPECT_FALSE((*r).is_unique);
    EXPECT_EQ((*r).free_vars.size(), 1u);
}

// ─── empty inputs ────────────────────────────────────────────────────────────

TEST(MatrixSolver, EmptyForms_Fails) {
    std::vector<LinearForm> forms;
    std::vector<std::string> vars = {"x"};
    MatrixSolver solver;
    auto r = solver.solve(forms, vars);
    EXPECT_FALSE(r.ok());
}

TEST(MatrixSolver, EmptyVars_Fails) {
    std::vector<LinearForm> forms = {make_form({{"x", 1.0}}, -1.0)};
    std::vector<std::string> vars;
    MatrixSolver solver;
    auto r = solver.solve(forms, vars);
    EXPECT_FALSE(r.ok());
}

// ─── rank metadata ───────────────────────────────────────────────────────────

TEST(MatrixSolver, RankReportedCorrectly) {
    std::vector<LinearForm> forms = {
        make_form({{"x", 1.0}, {"y", 0.0}}, -1.0), // x = 1
        make_form({{"x", 0.0}, {"y", 1.0}}, -2.0), // y = 2
    };
    std::vector<std::string> vars = {"x", "y"};
    MatrixSolver solver;
    auto r = solver.solve(forms, vars);
    ASSERT_TRUE(r.ok());
    EXPECT_EQ((*r).rank_A, 2);
    EXPECT_EQ((*r).rank_Ab, 2);
}

// ─── overdetermined consistent ───────────────────────────────────────────────

TEST(MatrixSolver, OverdeterminedConsistent) {
    // Three equations for two unknowns that are consistent
    // x = 1
    // y = 2
    // x + y = 3
    std::vector<LinearForm> forms = {
        make_form({{"x", 1.0}, {"y", 0.0}}, -1.0),
        make_form({{"x", 0.0}, {"y", 1.0}}, -2.0),
        make_form({{"x", 1.0}, {"y", 1.0}}, -3.0),
    };
    std::vector<std::string> vars = {"x", "y"};
    MatrixSolver solver;
    auto r = solver.solve(forms, vars);
    ASSERT_TRUE(r.ok());
    EXPECT_NEAR((*r).solutions["x"], 1.0, 1e-9);
    EXPECT_NEAR((*r).solutions["y"], 2.0, 1e-9);
}
