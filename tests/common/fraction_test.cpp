#include "../src/common/fraction.h"
#include <gtest/gtest.h>

using namespace math_solver;

TEST(FractionTest, Simplify) {
    Fraction frac1(4, 8);
    EXPECT_EQ(frac1.numerator, 1);
    EXPECT_EQ(frac1.denominator, 2);

    Fraction frac2(-6, -9);
    EXPECT_EQ(frac2.numerator, 2);
    EXPECT_EQ(frac2.denominator, 3);

    Fraction frac3(10, -15);
    EXPECT_EQ(frac3.numerator, -2);
    EXPECT_EQ(frac3.denominator, 3);
}

TEST(FractionTest, ToString) {
    Fraction frac1(3, 4);
    EXPECT_EQ(frac1.to_string(), "3/4");

    Fraction frac2(5, 1);
    EXPECT_EQ(frac2.to_string(), "5");

    Fraction frac3(-2, 3);
    EXPECT_EQ(frac3.to_string(), "-2/3");
}

TEST(FractionTest, ToLatex) {
    Fraction frac1(3, 4);
    EXPECT_EQ(frac1.to_latex(), "\\frac{3}{4}");

    Fraction frac2(5, 1);
    EXPECT_EQ(frac2.to_latex(), "5");

    Fraction frac3(-2, 3);
    EXPECT_EQ(frac3.to_latex(), "\\frac{-2}{3}");
}

TEST(FractionTest, ToDouble) {
    Fraction frac1(1, 2);
    EXPECT_DOUBLE_EQ(frac1.to_double(), 0.5);

    Fraction frac2(-3, 4);
    EXPECT_DOUBLE_EQ(frac2.to_double(), -0.75);

    Fraction frac3(5, -2);
    EXPECT_DOUBLE_EQ(frac3.to_double(), -2.5);
}

TEST(FractionTest, IsInteger) {
    Fraction frac1(4, 2);
    EXPECT_TRUE(frac1.is_integer());

    Fraction frac2(3, 1);
    EXPECT_TRUE(frac2.is_integer());

    Fraction frac3(5, 3);
    EXPECT_FALSE(frac3.is_integer());
}

TEST(FractionTest, DoubleToFraction) {
    Fraction frac1 = double_to_fraction(0.75);
    EXPECT_EQ(frac1.numerator, 3);
    EXPECT_EQ(frac1.denominator, 4);

    Fraction frac2 = double_to_fraction(-2.5);
    EXPECT_EQ(frac2.numerator, -5);
    EXPECT_EQ(frac2.denominator, 2);

    Fraction frac3 = double_to_fraction(0.3333333);
    EXPECT_EQ(frac3.numerator, 1);
    EXPECT_EQ(frac3.denominator, 3);
}

TEST(FractionTest, FormatCoefficient) {
    std::string coeff1 = format_coefficient(1.0, false, false);
    EXPECT_EQ(coeff1, "");

    std::string coeff2 = format_coefficient(-1.0, false, false);
    EXPECT_EQ(coeff2, "-");

    std::string coeff3 = format_coefficient(2.5, false, false);
    EXPECT_EQ(coeff3, "2.5");

    std::string coeff4 = format_coefficient(1.0, true, false);
    EXPECT_EQ(coeff4, "1");

    std::string coeff5 = format_coefficient(0.75, false, true);
    EXPECT_EQ(coeff5, "(3/4)");

    std::string coeff6 = format_coefficient(-1.0, true, true);
    EXPECT_EQ(coeff6, "-1");
}

// ============================================================
// Edge cases ที่ยังไม่ได้ทดสอบ
// ============================================================

// ทดสอบ Fraction เมื่อ denominator = 0 (ควร fallback เป็น 0/1)
TEST(FractionTest, ZeroDenominator) {
    Fraction frac(5, 0);
    EXPECT_EQ(frac.numerator, 0);
    EXPECT_EQ(frac.denominator, 1);
}

// ทดสอบ Fraction เมื่อ numerator = 0 (ต้อง simplify เป็น 0/1)
TEST(FractionTest, ZeroNumerator) {
    Fraction frac(0, 7);
    EXPECT_EQ(frac.numerator, 0);
    EXPECT_EQ(frac.denominator, 1);
}

// ทดสอบ Fraction ที่เป็นจำนวนเต็มพอดี (6/3 = 2/1)
TEST(FractionTest, SimplifiesWholeNumber) {
    Fraction frac(6, 3);
    EXPECT_EQ(frac.numerator, 2);
    EXPECT_EQ(frac.denominator, 1);
    EXPECT_TRUE(frac.is_integer());
    EXPECT_EQ(frac.to_string(), "2");
}

// ทดสอบ to_string / to_latex สำหรับ zero
TEST(FractionTest, ZeroToStringAndLatex) {
    Fraction frac(0, 5);
    EXPECT_EQ(frac.to_string(), "0");
    EXPECT_EQ(frac.to_latex(), "0");
    EXPECT_DOUBLE_EQ(frac.to_double(), 0.0);
}

// ทดสอบ double_to_fraction กับ NaN (ควร return 0/1)
TEST(FractionTest, DoubleToFractionNaN) {
    Fraction frac = double_to_fraction(std::nan(""));
    EXPECT_EQ(frac.numerator, 0);
    EXPECT_EQ(frac.denominator, 1);
}

// ทดสอบ double_to_fraction กับ Infinity (ควร return 0/1)
TEST(FractionTest, DoubleToFractionInfinity) {
    Fraction frac_pos =
        double_to_fraction(std::numeric_limits<double>::infinity());
    EXPECT_EQ(frac_pos.numerator, 0);
    EXPECT_EQ(frac_pos.denominator, 1);

    Fraction frac_neg =
        double_to_fraction(-std::numeric_limits<double>::infinity());
    EXPECT_EQ(frac_neg.numerator, 0);
    EXPECT_EQ(frac_neg.denominator, 1);
}

// ทดสอบ double_to_fraction กับจำนวนเต็ม (5.0 -> 5/1)
TEST(FractionTest, DoubleToFractionInteger) {
    Fraction frac = double_to_fraction(5.0);
    EXPECT_EQ(frac.numerator, 5);
    EXPECT_EQ(frac.denominator, 1);
}

// ทดสอบ double_to_fraction กับจำนวนลบ (-0.25 -> -1/4)
TEST(FractionTest, DoubleToFractionNegative) {
    Fraction frac = double_to_fraction(-0.25);
    EXPECT_EQ(frac.numerator, -1);
    EXPECT_EQ(frac.denominator, 4);
}

// ทดสอบ double_to_fraction กับค่า 0.0 (-> 0/1)
TEST(FractionTest, DoubleToFractionZero) {
    Fraction frac = double_to_fraction(0.0);
    EXPECT_EQ(frac.numerator, 0);
    EXPECT_EQ(frac.denominator, 1);
}

// ทดสอบ format_coefficient เมื่อ coeff = 0 (ไม่ใช่ ±1)
TEST(FractionTest, FormatCoefficientZero) {
    std::string result = format_coefficient(0.0, false, false);
    EXPECT_EQ(result, "0");
}

// ทดสอบ format_coefficient โหมด fraction กับค่าลบที่ไม่ใช่จำนวนเต็ม
TEST(FractionTest, FormatCoefficientNegativeFraction) {
    // -0.5 => fraction -1/2 => "(-1/2)"
    std::string result = format_coefficient(-0.5, false, true);
    EXPECT_EQ(result, "(-1/2)");
}

// ทดสอบ format_coefficient โหมด fraction เมื่อ show_one=true และเป็นจำนวนเต็ม
TEST(FractionTest, FormatCoefficientFractionShowOneInteger) {
    // 3.0 => fraction 3/1 => "3"
    std::string result = format_coefficient(3.0, true, true);
    EXPECT_EQ(result, "3");
}