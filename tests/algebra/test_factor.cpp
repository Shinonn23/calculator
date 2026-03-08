#include <gtest/gtest.h>

#include "algebra/polynomial/factor.hpp"

using namespace math_solver;

// ─── helpers ────────────────────────────────────────────────────────────────

static Polynomial linear(double a, const std::string& var, double b) {
    // a*var + b
    Polynomial p(a, var, 1);
    return p + Polynomial(b);
}

static Polynomial quadratic(double a, const std::string& var, double b, double c) {
    // a*var^2 + b*var + c
    Polynomial p(a, var, 2);
    p = p + Polynomial(b, var, 1);
    p = p + Polynomial(c);
    return p;
}

// ─── factor_polynomial ───────────────────────────────────────────────────────

TEST(FactorPolynomial, ZeroPolynomial) {
    FactoredForm ff = factor_polynomial(Polynomial());
    EXPECT_DOUBLE_EQ(ff.numeric_factor, 0.0);
    EXPECT_TRUE(ff.factors.empty());
}

TEST(FactorPolynomial, ConstantPolynomial) {
    FactoredForm ff = factor_polynomial(Polynomial(6.0));
    EXPECT_DOUBLE_EQ(ff.numeric_factor, 6.0);
    EXPECT_TRUE(ff.factors.empty());
    EXPECT_TRUE(ff.common_monomial.is_constant());
}

TEST(FactorPolynomial, LinearIrreducible) {
    // x + 1 — no factorization
    Polynomial p = linear(1.0, "x", 1.0);
    FactoredForm ff = factor_polynomial(p);
    EXPECT_TRUE(ff.is_trivial());
}

TEST(FactorPolynomial, ScalarExtraction) {
    // 4x + 2 → 2(2x + 1)
    Polynomial p = linear(4.0, "x", 2.0);
    FactoredForm ff = factor_polynomial(p);
    EXPECT_DOUBLE_EQ(ff.numeric_factor, 2.0);
    // The remaining factor should be 2x + 1
    ASSERT_FALSE(ff.factors.empty());
    EXPECT_DOUBLE_EQ(ff.factors[0].first.coeff_of_degree(1), 2.0);
    EXPECT_DOUBLE_EQ(ff.factors[0].first.coeff_of_degree(0), 1.0);
}

TEST(FactorPolynomial, MonomialGcdExtraction) {
    // x^2 + x → x(x + 1)
    Polynomial p(1.0, "x", 2);
    p = p + Polynomial(1.0, "x", 1);
    FactoredForm ff = factor_polynomial(p);
    EXPECT_EQ(ff.common_monomial.degree_of("x"), 1);
    ASSERT_FALSE(ff.factors.empty());
    // Factor after dividing by x should be x + 1
    EXPECT_DOUBLE_EQ(ff.factors[0].first.coeff_of_degree(1), 1.0);
    EXPECT_DOUBLE_EQ(ff.factors[0].first.coeff_of_degree(0), 1.0);
}

TEST(FactorPolynomial, QuadraticDifferenceOfSquares) {
    // x^2 - 1 = (x - 1)(x + 1)
    Polynomial p = quadratic(1.0, "x", 0.0, -1.0);
    FactoredForm ff = factor_polynomial(p);
    EXPECT_FALSE(ff.is_trivial());
    EXPECT_EQ(ff.factors.size(), 2u);
}

TEST(FactorPolynomial, QuadraticPerfectSquare) {
    // x^2 + 2x + 1 = (x + 1)^2
    Polynomial p = quadratic(1.0, "x", 2.0, 1.0);
    FactoredForm ff = factor_polynomial(p);
    EXPECT_FALSE(ff.is_trivial());
    ASSERT_EQ(ff.factors.size(), 1u);
    EXPECT_EQ(ff.factors[0].second, 2); // exponent 2
}

TEST(FactorPolynomial, QuadraticNegativeDiscriminant) {
    // x^2 + 1 — no real integer factorization
    Polynomial p = quadratic(1.0, "x", 0.0, 1.0);
    FactoredForm ff = factor_polynomial(p);
    EXPECT_TRUE(ff.is_trivial());
}

TEST(FactorPolynomial, QuadraticWithLeadingCoeff) {
    // 2x^2 - 8 = 2(x^2 - 4) = 2(x - 2)(x + 2)
    Polynomial p = quadratic(2.0, "x", 0.0, -8.0);
    FactoredForm ff = factor_polynomial(p);
    EXPECT_DOUBLE_EQ(ff.numeric_factor, 2.0);
    EXPECT_EQ(ff.factors.size(), 2u);
}

TEST(FactorPolynomial, HigherDegreeIrreducible) {
    // x^3 + x + 1 — no simple factorization
    Polynomial p(1.0, "x", 3);
    p = p + Polynomial(1.0, "x", 1);
    p = p + Polynomial(1.0);
    FactoredForm ff = factor_polynomial(p);
    // Should be returned as a single irreducible factor
    EXPECT_EQ(ff.factors.size(), 1u);
}

TEST(FactorPolynomial, NegativeLeadingCoefficientNormalized) {
    // -x^2 + 1 → -(x^2 - 1) = -(x-1)(x+1)
    Polynomial p = quadratic(-1.0, "x", 0.0, 1.0);
    FactoredForm ff = factor_polynomial(p);
    EXPECT_LT(ff.numeric_factor, 0.0); // negative numeric factor
}

// ─── FactoredForm::is_trivial ────────────────────────────────────────────────

TEST(FactoredForm, IsTrivialSingleFactor) {
    Polynomial p = linear(1.0, "x", 1.0);
    FactoredForm ff;
    ff.numeric_factor = 1.0;
    ff.factors.push_back({p, 1});
    EXPECT_TRUE(ff.is_trivial());
}

TEST(FactoredForm, NotTrivialTwoFactors) {
    Polynomial p = linear(1.0, "x", 1.0);
    FactoredForm ff;
    ff.numeric_factor = 1.0;
    ff.factors.push_back({p, 1});
    ff.factors.push_back({p, 1});
    EXPECT_FALSE(ff.is_trivial());
}

TEST(FactoredForm, NotTrivialNumericFactorNot1) {
    Polynomial p = linear(1.0, "x", 1.0);
    FactoredForm ff;
    ff.numeric_factor = 2.0;
    ff.factors.push_back({p, 1});
    EXPECT_FALSE(ff.is_trivial());
}

TEST(FactoredForm, NotTrivialWithMonomial) {
    Polynomial p = linear(1.0, "x", 1.0);
    FactoredForm ff;
    ff.numeric_factor = 1.0;
    ff.common_monomial = Monomial("x", 1);
    ff.factors.push_back({p, 1});
    EXPECT_FALSE(ff.is_trivial());
}

// ─── FactoredForm::to_string ─────────────────────────────────────────────────

TEST(FactoredForm, ToStringTrivial) {
    Polynomial p = linear(1.0, "x", 1.0);
    FactoredForm ff;
    ff.numeric_factor = 1.0;
    ff.factors.push_back({p, 1});
    EXPECT_EQ(ff.to_string(), p.to_string());
}

TEST(FactoredForm, ToStringWithNumericFactor) {
    Polynomial p = linear(1.0, "x", 1.0);
    FactoredForm ff;
    ff.numeric_factor = 2.0;
    ff.factors.push_back({p, 1});
    std::string s = ff.to_string();
    EXPECT_NE(s.find("2"), std::string::npos);
}

TEST(FactoredForm, ToStringSquareFactor) {
    Polynomial p = linear(1.0, "x", 1.0);
    FactoredForm ff;
    ff.numeric_factor = 1.0;
    ff.factors.push_back({p, 2});
    std::string s = ff.to_string();
    EXPECT_NE(s.find("^2"), std::string::npos);
}

// ─── try_factor_quadratic ────────────────────────────────────────────────────

TEST(TryFactorQuadratic, DifferenceOfSquares) {
    // x^2 - 1
    Polynomial p = quadratic(1.0, "x", 0.0, -1.0);
    std::vector<std::pair<Polynomial, int>> factors;
    bool ok = try_factor_quadratic(p, "x", factors);
    EXPECT_TRUE(ok);
    EXPECT_EQ(factors.size(), 2u);
}

TEST(TryFactorQuadratic, PerfectSquare) {
    // x^2 - 2x + 1 = (x - 1)^2
    Polynomial p = quadratic(1.0, "x", -2.0, 1.0);
    std::vector<std::pair<Polynomial, int>> factors;
    bool ok = try_factor_quadratic(p, "x", factors);
    EXPECT_TRUE(ok);
    ASSERT_EQ(factors.size(), 1u);
    EXPECT_EQ(factors[0].second, 2);
}

TEST(TryFactorQuadratic, NegativeDiscriminant) {
    // x^2 + x + 1 — irreducible over integers
    Polynomial p = quadratic(1.0, "x", 1.0, 1.0);
    std::vector<std::pair<Polynomial, int>> factors;
    bool ok = try_factor_quadratic(p, "x", factors);
    EXPECT_FALSE(ok);
}

TEST(TryFactorQuadratic, NonIntegerCoefficients) {
    // 1.5x^2 + 0.5x + 0.1
    Polynomial p = quadratic(1.5, "x", 0.5, 0.1);
    std::vector<std::pair<Polynomial, int>> factors;
    bool ok = try_factor_quadratic(p, "x", factors);
    EXPECT_FALSE(ok);
}
