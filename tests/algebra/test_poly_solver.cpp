#include <gtest/gtest.h>

#include "algebra/polynomial/polynomial.hpp"
#include "algebra/solver/poly_solver.hpp"

using namespace math_solver;

// ─── helpers ────────────────────────────────────────────────────────────────

static Polynomial linear_poly(double a, double b) {
    // a*x + b
    Polynomial p(a, "x", 1);
    return p + Polynomial(b);
}

static Polynomial quadratic_poly(double a, double b, double c) {
    Polynomial p(a, "x", 2);
    p = p + Polynomial(b, "x", 1);
    p = p + Polynomial(c);
    return p;
}

// ─── degree 0 ───────────────────────────────────────────────────────────────

TEST(PolynomialSolver, Degree0Zero_InfiniteSolutions) {
    // 0 = 0 → infinite solutions
    Polynomial p; // zero polynomial
    PolynomialSolver solver;
    auto r = solver.solve(p);
    EXPECT_FALSE(r.ok());
    // error message should mention "always" (always true)
    EXPECT_NE(r.error().message.find("always"), std::string::npos);
}

TEST(PolynomialSolver, Degree0Nonzero_NoSolution) {
    // 5 = 0 → no solution
    Polynomial p(5.0);
    PolynomialSolver solver;
    auto r = solver.solve(p);
    EXPECT_FALSE(r.ok());
}

// ─── degree 1 ───────────────────────────────────────────────────────────────

TEST(PolynomialSolver, Degree1SimpleRoot) {
    // x + 1 = 0 → x = -1
    Polynomial p = linear_poly(1.0, 1.0);
    PolynomialSolver solver;
    auto r = solver.solve(p);
    ASSERT_TRUE(r.ok());
    ASSERT_EQ((*r).real_roots.size(), 1u);
    EXPECT_NEAR((*r).real_roots[0], -1.0, 1e-9);
}

TEST(PolynomialSolver, Degree1PositiveRoot) {
    // x - 3 = 0 → x = 3
    Polynomial p = linear_poly(1.0, -3.0);
    PolynomialSolver solver;
    auto r = solver.solve(p);
    ASSERT_TRUE(r.ok());
    EXPECT_NEAR((*r).real_roots[0], 3.0, 1e-9);
}

TEST(PolynomialSolver, Degree1FractionalRoot) {
    // 2x - 3 = 0 → x = 1.5
    Polynomial p = linear_poly(2.0, -3.0);
    PolynomialSolver solver;
    auto r = solver.solve(p);
    ASSERT_TRUE(r.ok());
    EXPECT_NEAR((*r).real_roots[0], 1.5, 1e-9);
}

// ─── degree 2 ───────────────────────────────────────────────────────────────

TEST(PolynomialSolver, Degree2TwoRoots) {
    // x^2 - 1 = 0 → x = ±1
    Polynomial p = quadratic_poly(1.0, 0.0, -1.0);
    PolynomialSolver solver;
    auto r = solver.solve(p);
    ASSERT_TRUE(r.ok());
    ASSERT_EQ((*r).real_roots.size(), 2u);
    EXPECT_NEAR((*r).real_roots[0], -1.0, 1e-9);
    EXPECT_NEAR((*r).real_roots[1],  1.0, 1e-9);
}

TEST(PolynomialSolver, Degree2RepeatedRoot) {
    // (x - 2)^2 = x^2 - 4x + 4 → x = 2 (multiplicity 2)
    Polynomial p = quadratic_poly(1.0, -4.0, 4.0);
    PolynomialSolver solver;
    auto r = solver.solve(p);
    ASSERT_TRUE(r.ok());
    ASSERT_EQ((*r).real_roots.size(), 1u);
    EXPECT_NEAR((*r).real_roots[0], 2.0, 1e-9);
    EXPECT_EQ((*r).multiplicities[0], 2);
}

TEST(PolynomialSolver, Degree2ComplexRoots_NoSolution) {
    // x^2 + 1 = 0 — no real roots
    Polynomial p = quadratic_poly(1.0, 0.0, 1.0);
    PolynomialSolver solver;
    auto r = solver.solve(p);
    EXPECT_FALSE(r.ok());
}

TEST(PolynomialSolver, Degree2StandardQuadratic) {
    // x^2 - 5x + 6 = (x-2)(x-3) → x = 2, 3
    Polynomial p = quadratic_poly(1.0, -5.0, 6.0);
    PolynomialSolver solver;
    auto r = solver.solve(p);
    ASSERT_TRUE(r.ok());
    ASSERT_EQ((*r).real_roots.size(), 2u);
    EXPECT_NEAR((*r).real_roots[0], 2.0, 1e-9);
    EXPECT_NEAR((*r).real_roots[1], 3.0, 1e-9);
}

// ─── degree 3+ ───────────────────────────────────────────────────────────────

TEST(PolynomialSolver, Degree3ThreeRealRoots) {
    // x^3 - x = x(x-1)(x+1) → x = -1, 0, 1
    Polynomial p(1.0, "x", 3);
    p = p + Polynomial(-1.0, "x", 1);
    PolynomialSolver solver;
    auto r = solver.solve(p);
    ASSERT_TRUE(r.ok());
    EXPECT_EQ((*r).real_roots.size(), 3u);
    EXPECT_NEAR((*r).real_roots[0], -1.0, 1e-6);
    EXPECT_NEAR((*r).real_roots[1],  0.0, 1e-6);
    EXPECT_NEAR((*r).real_roots[2],  1.0, 1e-6);
}

TEST(PolynomialSolver, Degree3SingleRealRoot) {
    // x^3 - 8 = 0 → x = 2
    Polynomial p(1.0, "x", 3);
    p = p + Polynomial(-8.0);
    PolynomialSolver solver;
    auto r = solver.solve(p);
    ASSERT_TRUE(r.ok());
    // Should have exactly 1 real root (two complex conjugates)
    EXPECT_GE((*r).real_roots.size(), 1u);
    EXPECT_NEAR((*r).real_roots[0], 2.0, 1e-6);
}

// ─── multivariate ────────────────────────────────────────────────────────────

TEST(PolynomialSolver, Multivariate_Fails) {
    // xy — multivariate, not allowed
    Polynomial p(1.0, Monomial(std::map<std::string, int>{{"x", 1}, {"y", 1}}));
    PolynomialSolver solver;
    auto r = solver.solve(p);
    EXPECT_FALSE(r.ok());
}

// ─── PolyRoots ───────────────────────────────────────────────────────────────

TEST(PolyRoots, HasRealWhenNotEmpty) {
    PolyRoots r;
    EXPECT_FALSE(r.has_real());
    r.real_roots.push_back(1.0);
    EXPECT_TRUE(r.has_real());
}
