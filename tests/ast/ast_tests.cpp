// ============================================================
// Unit tests สำหรับ AST nodes ทั้งหมด
// ครอบคลุม: Number, Variable, BinaryOp, Equation, ExprVisitor
// ============================================================

#include "ast/binary.h"
#include "ast/equation.h"
#include "ast/expr.h"
#include "ast/function_call.h"
#include "ast/index_access.h"
#include "ast/number.h"
#include "ast/number_array.h"
#include "ast/variable.h"
#include <gtest/gtest.h>

using namespace math_solver;
using namespace std;

// ============================================================
// Number tests
// ============================================================

// ทดสอบ constructor กับค่าจำนวนเต็ม
TEST(NumberTest, IntegerValue) {
    Number num(42);
    EXPECT_DOUBLE_EQ(num.value(), 42.0);
}

// ทดสอบ constructor กับค่าทศนิยม
TEST(NumberTest, DecimalValue) {
    Number num(3.14);
    EXPECT_DOUBLE_EQ(num.value(), 3.14);
}

// ทดสอบ constructor กับค่าลบ
TEST(NumberTest, NegativeValue) {
    Number num(-7.5);
    EXPECT_DOUBLE_EQ(num.value(), -7.5);
}

// ทดสอบ constructor กับค่า 0
TEST(NumberTest, ZeroValue) {
    Number num(0);
    EXPECT_DOUBLE_EQ(num.value(), 0.0);
}

// ทดสอบ constructor ที่รับ Span ด้วย
TEST(NumberTest, ConstructorWithSpan) {
    Span   span(3, 5);
    Number num(10, span);
    EXPECT_DOUBLE_EQ(num.value(), 10.0);
    EXPECT_EQ(num.span().start, 3u);
    EXPECT_EQ(num.span().end, 5u);
}

// ทดสอบ to_string สำหรับจำนวนเต็ม (ไม่มี trailing zeros)
TEST(NumberTest, ToStringInteger) {
    Number num(42);
    EXPECT_EQ(num.to_string(), "42");
}

// ทดสอบ to_string สำหรับทศนิยม
TEST(NumberTest, ToStringDecimal) {
    Number num(3.14);
    EXPECT_EQ(num.to_string(), "3.14");
}

// ทดสอบ to_string สำหรับค่า 0
TEST(NumberTest, ToStringZero) {
    Number num(0);
    EXPECT_EQ(num.to_string(), "0");
}

// ทดสอบ clone — ต้องได้ค่าเท่ากันแต่เป็น object ใหม่
TEST(NumberTest, Clone) {
    Number num(99, Span(1, 3));
    auto   cloned     = num.clone();

    // ต้องเป็น Number ด้วย
    auto*  cloned_num = dynamic_cast<Number*>(cloned.get());
    ASSERT_NE(cloned_num, nullptr);
    EXPECT_DOUBLE_EQ(cloned_num->value(), 99.0);
    EXPECT_EQ(cloned_num->span().start, 1u);
    EXPECT_EQ(cloned_num->span().end, 3u);
}

// ทดสอบ accept — visitor ต้องถูกเรียก
TEST(NumberTest, AcceptVisitor) {
    // สร้าง mock visitor ง่ายๆ ที่เก็บว่าเรียก visit(Number) แล้ว
    struct TestVisitor : ExprVisitor {
        bool visited_number = false;
        void visit(const Number&) override { visited_number = true; }
        void visit(const BinaryOp&) override {}
        void visit(const Variable&) override {}
        void visit(const FunctionCall&) override {}
        void visit(const NumberArray&) override {}
        void visit(const IndexAccess&) override {}
    };

    Number      num(5);
    TestVisitor visitor;
    num.accept(visitor);
    EXPECT_TRUE(visitor.visited_number);
}

// ============================================================
// Variable tests
// ============================================================

// ทดสอบ constructor กับชื่อตัวแปร
TEST(VariableTest, BasicName) {
    Variable var("x");
    EXPECT_EQ(var.name(), "x");
}

// ทดสอบ constructor ที่มีชื่อยาว
TEST(VariableTest, LongName) {
    Variable var("my_variable");
    EXPECT_EQ(var.name(), "my_variable");
}

// ทดสอบ constructor ที่รับ Span
TEST(VariableTest, ConstructorWithSpan) {
    Span     span(0, 1);
    Variable var("y", span);
    EXPECT_EQ(var.name(), "y");
    EXPECT_EQ(var.span().start, 0u);
    EXPECT_EQ(var.span().end, 1u);
}

// ทดสอบ to_string — ต้องเป็นชื่อตัวแปร
TEST(VariableTest, ToString) {
    Variable var("abc");
    EXPECT_EQ(var.to_string(), "abc");
}

// ทดสอบ clone
TEST(VariableTest, Clone) {
    Variable var("z", Span(2, 3));
    auto     cloned     = var.clone();

    auto*    cloned_var = dynamic_cast<Variable*>(cloned.get());
    ASSERT_NE(cloned_var, nullptr);
    EXPECT_EQ(cloned_var->name(), "z");
    EXPECT_EQ(cloned_var->span().start, 2u);
}

// ทดสอบ accept visitor
TEST(VariableTest, AcceptVisitor) {
    struct TestVisitor : ExprVisitor {
        bool visited_variable = false;
        void visit(const Number&) override {}
        void visit(const BinaryOp&) override {}
        void visit(const Variable&) override { visited_variable = true; }
        void visit(const FunctionCall&) override {}
        void visit(const NumberArray&) override {}
        void visit(const IndexAccess&) override {}
    };

    Variable    var("x");
    TestVisitor visitor;
    var.accept(visitor);
    EXPECT_TRUE(visitor.visited_variable);
}

// ============================================================
// BinaryOp tests
// ============================================================

// ทดสอบ constructor กับ Add operation
TEST(BinaryOpTest, AddOperation) {
    auto     left  = make_unique<Number>(2);
    auto     right = make_unique<Number>(3);
    BinaryOp op(move(left), move(right), BinaryOpType::Add);

    EXPECT_EQ(op.op(), BinaryOpType::Add);
}

// ทดสอบ to_string สำหรับทุก operator
TEST(BinaryOpTest, ToStringAdd) {
    auto     left  = make_unique<Number>(2);
    auto     right = make_unique<Number>(3);
    BinaryOp op(move(left), move(right), BinaryOpType::Add);
    EXPECT_EQ(op.to_string(), "2 + 3");
}

TEST(BinaryOpTest, ToStringSub) {
    auto     left  = make_unique<Number>(5);
    auto     right = make_unique<Number>(1);
    BinaryOp op(move(left), move(right), BinaryOpType::Sub);
    EXPECT_EQ(op.to_string(), "5 - 1");
}

TEST(BinaryOpTest, ToStringMul) {
    auto     left  = make_unique<Number>(4);
    auto     right = make_unique<Number>(6);
    BinaryOp op(move(left), move(right), BinaryOpType::Mul);
    EXPECT_EQ(op.to_string(), "4*6");
}

TEST(BinaryOpTest, ToStringDiv) {
    auto     left  = make_unique<Number>(10);
    auto     right = make_unique<Number>(2);
    BinaryOp op(move(left), move(right), BinaryOpType::Div);
    EXPECT_EQ(op.to_string(), "10/2");
}

TEST(BinaryOpTest, ToStringPow) {
    auto     left  = make_unique<Number>(2);
    auto     right = make_unique<Number>(3);
    BinaryOp op(move(left), move(right), BinaryOpType::Pow);
    EXPECT_EQ(op.to_string(), "2^3");
}

// ทดสอบ left() และ right() accessors
TEST(BinaryOpTest, ChildAccessors) {
    auto     left  = make_unique<Number>(7);
    auto     right = make_unique<Variable>("x");
    BinaryOp op(move(left), move(right), BinaryOpType::Mul);

    // left ต้องเป็น Number
    auto*    l = dynamic_cast<const Number*>(&op.left());
    ASSERT_NE(l, nullptr);
    EXPECT_DOUBLE_EQ(l->value(), 7.0);

    // right ต้องเป็น Variable
    auto* r = dynamic_cast<const Variable*>(&op.right());
    ASSERT_NE(r, nullptr);
    EXPECT_EQ(r->name(), "x");
}

// ทดสอบ span merge อัตโนมัติจาก children
TEST(BinaryOpTest, SpanAutoMerge) {
    auto     left  = make_unique<Number>(1, Span(0, 1));
    auto     right = make_unique<Number>(2, Span(4, 5));
    BinaryOp op(move(left), move(right), BinaryOpType::Add);

    // span ต้อง merge จาก left.start ถึง right.end
    EXPECT_EQ(op.span().start, 0u);
    EXPECT_EQ(op.span().end, 5u);
}

// ทดสอบ constructor ที่ระบุ Span เอง
TEST(BinaryOpTest, ExplicitSpan) {
    auto     left  = make_unique<Number>(1);
    auto     right = make_unique<Number>(2);
    Span     custom_span(10, 20);
    BinaryOp op(move(left), move(right), BinaryOpType::Sub, custom_span);

    EXPECT_EQ(op.span().start, 10u);
    EXPECT_EQ(op.span().end, 20u);
}

// ทดสอบ clone — deep copy ทั้ง tree
TEST(BinaryOpTest, Clone) {
    auto     left  = make_unique<Number>(3, Span(0, 1));
    auto     right = make_unique<Variable>("y", Span(4, 5));
    BinaryOp op(move(left), move(right), BinaryOpType::Mul);

    auto     cloned    = op.clone();
    auto*    cloned_op = dynamic_cast<BinaryOp*>(cloned.get());
    ASSERT_NE(cloned_op, nullptr);
    EXPECT_EQ(cloned_op->op(), BinaryOpType::Mul);
    EXPECT_EQ(cloned_op->to_string(), "3*y");
}

// ทดสอบ nested expression: (1 + 2) * 3
TEST(BinaryOpTest, NestedExpression) {
    auto one   = make_unique<Number>(1);
    auto two   = make_unique<Number>(2);
    auto add   = make_unique<BinaryOp>(move(one), move(two), BinaryOpType::Add);
    auto three = make_unique<Number>(3);
    BinaryOp mul(move(add), move(three), BinaryOpType::Mul);

    EXPECT_EQ(mul.to_string(), "(1 + 2)*3");
}

// ทดสอบ accept visitor
TEST(BinaryOpTest, AcceptVisitor) {
    struct TestVisitor : ExprVisitor {
        bool visited_binary = false;
        void visit(const Number&) override {}
        void visit(const BinaryOp&) override { visited_binary = true; }
        void visit(const Variable&) override {}
        void visit(const FunctionCall&) override {}
        void visit(const NumberArray&) override {}
        void visit(const IndexAccess&) override {}
    };

    auto        left  = make_unique<Number>(1);
    auto        right = make_unique<Number>(2);
    BinaryOp    op(move(left), move(right), BinaryOpType::Add);
    TestVisitor visitor;
    op.accept(visitor);
    EXPECT_TRUE(visitor.visited_binary);
}

// ============================================================
// Equation tests
// ============================================================

// ทดสอบ constructor พื้นฐาน
TEST(EquationTest, BasicEquation) {
    auto     lhs = make_unique<Variable>("x");
    auto     rhs = make_unique<Number>(5);
    Equation eq(move(lhs), move(rhs));

    EXPECT_EQ(eq.to_string(), "x = 5");
}

// ทดสอบ lhs() / rhs() accessors
TEST(EquationTest, Accessors) {
    auto     lhs = make_unique<Number>(10);
    auto     rhs = make_unique<Variable>("y");
    Equation eq(move(lhs), move(rhs));

    auto*    l = dynamic_cast<const Number*>(&eq.lhs());
    ASSERT_NE(l, nullptr);
    EXPECT_DOUBLE_EQ(l->value(), 10.0);

    auto* r = dynamic_cast<const Variable*>(&eq.rhs());
    ASSERT_NE(r, nullptr);
    EXPECT_EQ(r->name(), "y");
}

// ทดสอบ span auto merge จาก lhs/rhs
TEST(EquationTest, SpanAutoMerge) {
    auto     lhs = make_unique<Number>(1, Span(0, 1));
    auto     rhs = make_unique<Number>(2, Span(4, 5));
    Equation eq(move(lhs), move(rhs));

    EXPECT_EQ(eq.span().start, 0u);
    EXPECT_EQ(eq.span().end, 5u);
}

// ทดสอบ constructor ที่ระบุ Span เอง
TEST(EquationTest, ExplicitSpan) {
    auto     lhs = make_unique<Number>(1);
    auto     rhs = make_unique<Number>(2);
    Equation eq(move(lhs), move(rhs), Span(0, 10));

    EXPECT_EQ(eq.span().start, 0u);
    EXPECT_EQ(eq.span().end, 10u);
}

// ทดสอบ clone — deep copy
TEST(EquationTest, Clone) {
    auto     lhs = make_unique<Variable>("a");
    auto     rhs = make_unique<Number>(7);
    Equation eq(move(lhs), move(rhs), Span(0, 5));

    auto     cloned = eq.clone();
    EXPECT_EQ(cloned->to_string(), "a = 7");
    EXPECT_EQ(cloned->span().start, 0u);
    EXPECT_EQ(cloned->span().end, 5u);
}

// ทดสอบ to_string กับ expression ที่ซับซ้อน
TEST(EquationTest, ComplexToString) {
    // 2x + 1 = 5
    auto two  = make_unique<Number>(2);
    auto x    = make_unique<Variable>("x");
    auto mul  = make_unique<BinaryOp>(move(two), move(x), BinaryOpType::Mul);
    auto one  = make_unique<Number>(1);
    auto add  = make_unique<BinaryOp>(move(mul), move(one), BinaryOpType::Add);
    auto five = make_unique<Number>(5);
    Equation eq(move(add), move(five));

    EXPECT_EQ(eq.to_string(), "2*x + 1 = 5");
}

// ทดสอบ take_lhs / take_rhs — move ownership ออก
TEST(EquationTest, TakeOwnership) {
    auto     lhs = make_unique<Number>(10);
    auto     rhs = make_unique<Variable>("z");
    Equation eq(move(lhs), move(rhs));

    auto     taken_lhs = eq.take_lhs();
    ASSERT_NE(taken_lhs, nullptr);

    auto* num = dynamic_cast<Number*>(taken_lhs.get());
    ASSERT_NE(num, nullptr);
    EXPECT_DOUBLE_EQ(num->value(), 10.0);
}

// ============================================================
// Expr base class tests
// ============================================================

// ทดสอบ set_span()
TEST(ExprTest, SetSpan) {
    Number num(5);
    EXPECT_EQ(num.span().start, 0u);

    num.set_span(Span(10, 20));
    EXPECT_EQ(num.span().start, 10u);
    EXPECT_EQ(num.span().end, 20u);
}
