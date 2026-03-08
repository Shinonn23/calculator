#include <gtest/gtest.h>

#include "algebra/polynomial/polynomial.hpp"

using namespace math_solver;

// ============================================================
// Monomial
// ============================================================

TEST(Monomial, DefaultIsConstant) {
    Monomial m;
    EXPECT_TRUE(m.is_constant());
    EXPECT_EQ(m.total_degree(), 0);
    EXPECT_EQ(m.to_string(), "");
}

TEST(Monomial, SingleVarDegreeOne) {
    Monomial m("x");
    EXPECT_FALSE(m.is_constant());
    EXPECT_EQ(m.total_degree(), 1);
    EXPECT_EQ(m.degree_of("x"), 1);
    EXPECT_EQ(m.degree_of("y"), 0);
    EXPECT_EQ(m.to_string(), "x");
}

TEST(Monomial, SingleVarHighDegree) {
    Monomial m("x", 3);
    EXPECT_EQ(m.total_degree(), 3);
    EXPECT_EQ(m.degree_of("x"), 3);
    EXPECT_EQ(m.to_string(), "x^3");
}

TEST(Monomial, ZeroExponentBecomesConstant) {
    Monomial m("x", 0);
    EXPECT_TRUE(m.is_constant());
}

TEST(Monomial, MultiVarFromMap) {
    Monomial m(std::map<std::string, int>{{"x", 2}, {"y", 1}});
    EXPECT_EQ(m.total_degree(), 3);
    EXPECT_EQ(m.degree_of("x"), 2);
    EXPECT_EQ(m.degree_of("y"), 1);
}

TEST(Monomial, MapWithZeroExponentDropped) {
    Monomial m(std::map<std::string, int>{{"x", 2}, {"y", 0}});
    EXPECT_EQ(m.total_degree(), 2);
    EXPECT_EQ(m.degree_of("y"), 0);
}

TEST(Monomial, VariableNames) {
    Monomial m(std::map<std::string, int>{{"a", 1}, {"b", 2}});
    auto names = m.variable_names();
    ASSERT_EQ(names.size(), 2u);
    EXPECT_EQ(names[0], "a");
    EXPECT_EQ(names[1], "b");
}

TEST(Monomial, Multiply) {
    Monomial x("x", 2);
    Monomial y("y", 1);
    Monomial xy = x * y;
    EXPECT_EQ(xy.degree_of("x"), 2);
    EXPECT_EQ(xy.degree_of("y"), 1);
    EXPECT_EQ(xy.total_degree(), 3);
}

TEST(Monomial, MultiplyMergesExponents) {
    Monomial x1("x", 1);
    Monomial x2("x", 2);
    Monomial r = x1 * x2;
    EXPECT_EQ(r.degree_of("x"), 3);
}

TEST(Monomial, MultiplyZeroExpResultPruned) {
    // x^1 * x^(-1) should give constant monomial (implementation note:
    // this tests pruning when exponents sum to zero)
    Monomial x1("x", 2);
    Monomial xm1(std::map<std::string, int>{{"x", -2}});
    Monomial r = x1 * xm1;
    EXPECT_TRUE(r.is_constant());
}

TEST(Monomial, Divide) {
    Monomial x3("x", 3);
    Monomial x1("x", 1);
    Monomial r = x3 / x1;
    EXPECT_EQ(r.degree_of("x"), 2);
}

TEST(Monomial, DivisibleBy) {
    Monomial x3y2(std::map<std::string, int>{{"x", 3}, {"y", 2}});
    Monomial xy(std::map<std::string, int>{{"x", 1}, {"y", 1}});
    EXPECT_TRUE(x3y2.divisible_by(xy));
    EXPECT_FALSE(xy.divisible_by(x3y2));
}

TEST(Monomial, DivisibleByConstant) {
    Monomial x("x");
    Monomial one;
    EXPECT_TRUE(x.divisible_by(one));
}

TEST(Monomial, Pow) {
    Monomial x("x", 1);
    Monomial x3 = x.pow(3);
    EXPECT_EQ(x3.degree_of("x"), 3);
}

TEST(Monomial, PowZero) {
    Monomial x("x", 2);
    Monomial r = x.pow(0);
    EXPECT_TRUE(r.is_constant());
}

TEST(Monomial, EqualityAndInequality) {
    Monomial a("x", 2);
    Monomial b("x", 2);
    Monomial c("x", 3);
    EXPECT_EQ(a, b);
    EXPECT_NE(a, c);
}

TEST(Monomial, LessOperatorGraded) {
    // Higher total degree sorts first (less than in the map sense)
    Monomial x2("x", 2);
    Monomial x1("x", 1);
    EXPECT_TRUE(x2 < x1); // degree 2 > degree 1 → sorts first
    EXPECT_FALSE(x1 < x2);
}

TEST(Monomial, ToStringMultivar) {
    Monomial m(std::map<std::string, int>{{"x", 2}, {"y", 3}});
    std::string s = m.to_string();
    EXPECT_NE(s.find("x^2"), std::string::npos);
    EXPECT_NE(s.find("y^3"), std::string::npos);
}

// ============================================================
// Polynomial
// ============================================================

TEST(Polynomial, DefaultIsZero) {
    Polynomial p;
    EXPECT_TRUE(p.is_zero());
    EXPECT_TRUE(p.is_constant());
    EXPECT_EQ(p.to_string(), "0");
    EXPECT_EQ(p.degree(), 0);
    EXPECT_EQ(p.num_terms(), 0u);
}

TEST(Polynomial, ConstantConstruction) {
    Polynomial p(5.0);
    EXPECT_FALSE(p.is_zero());
    EXPECT_TRUE(p.is_constant());
    EXPECT_DOUBLE_EQ(p.constant_value(), 5.0);
    EXPECT_EQ(p.degree(), 0);
}

TEST(Polynomial, NearZeroConstantBecomesZero) {
    Polynomial p(1e-15);
    EXPECT_TRUE(p.is_zero());
}

TEST(Polynomial, SingleTermConstruction) {
    Polynomial p(3.0, "x", 2);
    EXPECT_FALSE(p.is_zero());
    EXPECT_FALSE(p.is_constant());
    EXPECT_EQ(p.degree(), 2);
    EXPECT_EQ(p.num_terms(), 1u);
    EXPECT_DOUBLE_EQ(p.coefficient(Monomial("x", 2)), 3.0);
}

TEST(Polynomial, SingleTermMonomialConstruction) {
    Monomial m("x", 1);
    Polynomial p(2.0, m);
    EXPECT_DOUBLE_EQ(p.coefficient(m), 2.0);
}

TEST(Polynomial, Variables) {
    Polynomial p(1.0, "x", 2);
    p = p + Polynomial(1.0, "y", 1);
    auto vars = p.variables();
    ASSERT_EQ(vars.size(), 2u);
    EXPECT_EQ(vars[0], "x");
    EXPECT_EQ(vars[1], "y");
}

TEST(Polynomial, IsUnivariate) {
    Polynomial p(1.0, "x", 2);
    p = p + Polynomial(1.0, "x", 1);
    EXPECT_TRUE(p.is_univariate());
}

TEST(Polynomial, IsNotUnivariateMultivar) {
    Polynomial p(1.0, "x", 1);
    p = p + Polynomial(1.0, "y", 1);
    EXPECT_FALSE(p.is_univariate());
}

TEST(Polynomial, SingleVariable) {
    Polynomial p(1.0, "z", 3);
    EXPECT_EQ(p.single_variable(), "z");
}

TEST(Polynomial, SingleVariableEmpty) {
    Polynomial p;
    EXPECT_EQ(p.single_variable(), "x"); // default
}

TEST(Polynomial, CoeffOfDegree) {
    Polynomial p(2.0, "x", 2);
    p = p + Polynomial(3.0, "x", 1);
    p = p + Polynomial(-1.0);
    EXPECT_DOUBLE_EQ(p.coeff_of_degree(2), 2.0);
    EXPECT_DOUBLE_EQ(p.coeff_of_degree(1), 3.0);
    EXPECT_DOUBLE_EQ(p.coeff_of_degree(0), -1.0);
    EXPECT_DOUBLE_EQ(p.coeff_of_degree(5), 0.0);
}

TEST(Polynomial, Addition) {
    Polynomial a(1.0, "x", 1);
    Polynomial b(2.0, "x", 1);
    Polynomial c = a + b;
    EXPECT_DOUBLE_EQ(c.coefficient(Monomial("x", 1)), 3.0);
}

TEST(Polynomial, AdditionCancelsToZero) {
    Polynomial a(1.0, "x", 1);
    Polynomial b(-1.0, "x", 1);
    Polynomial c = a + b;
    EXPECT_TRUE(c.is_zero());
}

TEST(Polynomial, Subtraction) {
    Polynomial a(5.0, "x", 1);
    Polynomial b(2.0, "x", 1);
    Polynomial c = a - b;
    EXPECT_DOUBLE_EQ(c.coefficient(Monomial("x", 1)), 3.0);
}

TEST(Polynomial, Negation) {
    Polynomial p(3.0, "x", 1);
    Polynomial neg = -p;
    EXPECT_DOUBLE_EQ(neg.coefficient(Monomial("x", 1)), -3.0);
}

TEST(Polynomial, MultiplicationByPolynomial) {
    // (x + 1) * (x - 1) = x^2 - 1
    Polynomial f(1.0, "x", 1);
    f = f + Polynomial(1.0);
    Polynomial g(1.0, "x", 1);
    g = g + Polynomial(-1.0);
    Polynomial r = f * g;
    EXPECT_DOUBLE_EQ(r.coefficient(Monomial("x", 2)), 1.0);
    EXPECT_DOUBLE_EQ(r.coeff_of_degree(0), -1.0);
    EXPECT_DOUBLE_EQ(r.coeff_of_degree(1), 0.0); // x^1 term cancels
}

TEST(Polynomial, MultiplicationByScalar) {
    Polynomial p(3.0, "x", 1);
    Polynomial r = p * 2.0;
    EXPECT_DOUBLE_EQ(r.coefficient(Monomial("x", 1)), 6.0);
}

TEST(Polynomial, MultiplicationByZeroScalar) {
    Polynomial p(3.0, "x", 1);
    Polynomial r = p * 0.0;
    EXPECT_TRUE(r.is_zero());
}

TEST(Polynomial, DivisionByScalar) {
    Polynomial p(6.0, "x", 1);
    Polynomial r = p / 2.0;
    EXPECT_DOUBLE_EQ(r.coefficient(Monomial("x", 1)), 3.0);
}

TEST(Polynomial, PowZero) {
    Polynomial p(3.0, "x", 1);
    Polynomial r = p.pow(0);
    EXPECT_TRUE(r.is_constant());
    EXPECT_DOUBLE_EQ(r.constant_value(), 1.0);
}

TEST(Polynomial, PowOne) {
    Polynomial p(2.0, "x", 1);
    Polynomial r = p.pow(1);
    EXPECT_DOUBLE_EQ(r.coefficient(Monomial("x", 1)), 2.0);
}

TEST(Polynomial, PowTwo) {
    // (x + 1)^2 = x^2 + 2x + 1
    Polynomial p(1.0, "x", 1);
    p = p + Polynomial(1.0);
    Polynomial r = p.pow(2);
    EXPECT_DOUBLE_EQ(r.coeff_of_degree(2), 1.0);
    EXPECT_DOUBLE_EQ(r.coeff_of_degree(1), 2.0);
    EXPECT_DOUBLE_EQ(r.coeff_of_degree(0), 1.0);
}

TEST(Polynomial, CoefficientGcdAllInteger) {
    // 4x^2 + 6x + 2 → gcd = 2
    Polynomial p(4.0, "x", 2);
    p = p + Polynomial(6.0, "x", 1);
    p = p + Polynomial(2.0);
    EXPECT_DOUBLE_EQ(p.coefficient_gcd(), 2.0);
}

TEST(Polynomial, CoefficientGcdNonInteger) {
    Polynomial p(1.5, "x", 1);
    EXPECT_DOUBLE_EQ(p.coefficient_gcd(), 1.0);
}

TEST(Polynomial, CoefficientGcdZero) {
    Polynomial p;
    EXPECT_DOUBLE_EQ(p.coefficient_gcd(), 1.0);
}

TEST(Polynomial, MonomialGcdSimple) {
    // x^2 + x → gcd is x
    Polynomial p(1.0, "x", 2);
    p = p + Polynomial(1.0, "x", 1);
    Monomial g = p.monomial_gcd();
    EXPECT_EQ(g.degree_of("x"), 1);
}

TEST(Polynomial, MonomialGcdConstantTerm) {
    // x^2 + 1 → gcd is 1 (constant)
    Polynomial p(1.0, "x", 2);
    p = p + Polynomial(1.0);
    Monomial g = p.monomial_gcd();
    EXPECT_TRUE(g.is_constant());
}

TEST(Polynomial, DivideByMonomial) {
    // x^2 + x divided by x → x + 1
    Polynomial p(1.0, "x", 2);
    p = p + Polynomial(1.0, "x", 1);
    Polynomial r = p.divide_by_monomial(Monomial("x", 1));
    EXPECT_DOUBLE_EQ(r.coeff_of_degree(1), 1.0);
    EXPECT_DOUBLE_EQ(r.coeff_of_degree(0), 1.0);
}

TEST(Polynomial, ToStringZero) {
    EXPECT_EQ(Polynomial().to_string(), "0");
}

TEST(Polynomial, ToStringConstant) {
    EXPECT_EQ(Polynomial(3.0).to_string(), "3");
}

TEST(Polynomial, ToStringLinear) {
    Polynomial p(1.0, "x", 1);
    EXPECT_EQ(p.to_string(), "x");
}

TEST(Polynomial, ToStringWithCoefficient) {
    Polynomial p(2.0, "x", 1);
    EXPECT_EQ(p.to_string(), "2x");
}

TEST(Polynomial, ToStringNegativeLeading) {
    Polynomial p(-1.0, "x", 1);
    EXPECT_EQ(p.to_string(), "-x");
}

TEST(Polynomial, ToStringMultiTerm) {
    Polynomial p(1.0, "x", 2);
    p = p + Polynomial(-1.0);
    std::string s = p.to_string();
    EXPECT_NE(s.find("x^2"), std::string::npos);
    EXPECT_NE(s.find("1"), std::string::npos);
}
