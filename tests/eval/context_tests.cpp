// ============================================================
// Unit tests สำหรับ Context (variable storage)
// ครอบคลุม: set, get, has, unset, clear, all_names, all,
//           size, empty
// ============================================================

#include "eval/context.h"
#include <algorithm>
#include <gtest/gtest.h>
#include <stdexcept>

using namespace math_solver;
using namespace std;

// ============================================================
// set / get / has
// ============================================================

// ทดสอบ set แล้ว get กลับ
TEST(ContextTest, SetAndGet) {
    Context ctx;
    ctx.set("x", 42);
    EXPECT_DOUBLE_EQ(ctx.get("x"), 42.0);
}

// ทดสอบ has ว่ามีตัวแปร
TEST(ContextTest, HasExists) {
    Context ctx;
    ctx.set("y", 10);
    EXPECT_TRUE(ctx.has("y"));
}

// ทดสอบ has ว่าไม่มีตัวแปร
TEST(ContextTest, HasNotExists) {
    Context ctx;
    EXPECT_FALSE(ctx.has("z"));
}

// ทดสอบ get ตัวแปรที่ไม่มี — ต้อง throw
TEST(ContextTest, GetUndefined) {
    Context ctx;
    EXPECT_THROW(ctx.get("missing"), runtime_error);
}

// ทดสอบ set ทับค่าเดิม (overwrite)
TEST(ContextTest, OverwriteValue) {
    Context ctx;
    ctx.set("x", 1);
    ctx.set("x", 99);
    EXPECT_DOUBLE_EQ(ctx.get("x"), 99.0);
}

// ทดสอบ set ค่าลบ
TEST(ContextTest, SetNegativeValue) {
    Context ctx;
    ctx.set("n", -3.5);
    EXPECT_DOUBLE_EQ(ctx.get("n"), -3.5);
}

// ทดสอบ set ค่า 0
TEST(ContextTest, SetZero) {
    Context ctx;
    ctx.set("z", 0);
    EXPECT_DOUBLE_EQ(ctx.get("z"), 0.0);
    EXPECT_TRUE(ctx.has("z"));
}

// ============================================================
// unset
// ============================================================

// ทดสอบ unset ตัวแปรที่มีอยู่ — ต้อง return true
TEST(ContextTest, UnsetExists) {
    Context ctx;
    ctx.set("x", 5);
    EXPECT_TRUE(ctx.unset("x"));
    EXPECT_FALSE(ctx.has("x"));
}

// ทดสอบ unset ตัวแปรที่ไม่มี — ต้อง return false
TEST(ContextTest, UnsetNotExists) {
    Context ctx;
    EXPECT_FALSE(ctx.unset("missing"));
}

// ทดสอบ get หลัง unset — ต้อง throw
TEST(ContextTest, GetAfterUnset) {
    Context ctx;
    ctx.set("x", 10);
    ctx.unset("x");
    EXPECT_THROW(ctx.get("x"), runtime_error);
}

// ============================================================
// clear
// ============================================================

// ทดสอบ clear ลบทุกตัวแปร
TEST(ContextTest, ClearAll) {
    Context ctx;
    ctx.set("a", 1);
    ctx.set("b", 2);
    ctx.set("c", 3);
    ctx.clear();
    EXPECT_TRUE(ctx.empty());
    EXPECT_EQ(ctx.size(), 0u);
}

// ทดสอบ clear เมื่อว่างอยู่แล้ว (ไม่ crash)
TEST(ContextTest, ClearEmpty) {
    Context ctx;
    ctx.clear(); // ไม่ควร crash
    EXPECT_TRUE(ctx.empty());
}

// ============================================================
// all_names
// ============================================================

// ทดสอบ all_names ดึงชื่อตัวแปรทั้งหมด
TEST(ContextTest, AllNames) {
    Context ctx;
    ctx.set("x", 1);
    ctx.set("y", 2);

    auto names = ctx.all_names();
    EXPECT_EQ(names.size(), 2u);

    // เช็คว่ามีทั้ง x และ y (ลำดับไม่แน่นอนเพราะ unordered_map)
    sort(names.begin(), names.end());
    EXPECT_EQ(names[0], "x");
    EXPECT_EQ(names[1], "y");
}

// ทดสอบ all_names เมื่อว่าง
TEST(ContextTest, AllNamesEmpty) {
    Context ctx;
    auto    names = ctx.all_names();
    EXPECT_TRUE(names.empty());
}

// ============================================================
// all
// ============================================================

// ทดสอบ all() return reference ไปยัง internal map
TEST(ContextTest, AllMap) {
    Context ctx;
    ctx.set("a", 10);
    ctx.set("b", 20);

    const auto& m = ctx.all();
    EXPECT_EQ(m.size(), 2u);
    EXPECT_DOUBLE_EQ(m.at("a"), 10.0);
    EXPECT_DOUBLE_EQ(m.at("b"), 20.0);
}

// ============================================================
// size / empty
// ============================================================

// ทดสอบ size เริ่มต้น = 0
TEST(ContextTest, InitialSize) {
    Context ctx;
    EXPECT_EQ(ctx.size(), 0u);
}

// ทดสอบ empty เริ่มต้น = true
TEST(ContextTest, InitialEmpty) {
    Context ctx;
    EXPECT_TRUE(ctx.empty());
}

// ทดสอบ size เพิ่มขึ้นหลัง set
TEST(ContextTest, SizeIncrease) {
    Context ctx;
    ctx.set("a", 1);
    EXPECT_EQ(ctx.size(), 1u);
    ctx.set("b", 2);
    EXPECT_EQ(ctx.size(), 2u);
}

// ทดสอบ empty = false เมื่อมีตัวแปร
TEST(ContextTest, NotEmptyAfterSet) {
    Context ctx;
    ctx.set("x", 1);
    EXPECT_FALSE(ctx.empty());
}

// ทดสอบ size ลดลงหลัง unset
TEST(ContextTest, SizeDecrease) {
    Context ctx;
    ctx.set("a", 1);
    ctx.set("b", 2);
    ctx.unset("a");
    EXPECT_EQ(ctx.size(), 1u);
}

// ทดสอบ set ชื่อเดิมไม่เพิ่ม size
TEST(ContextTest, OverwriteDoesNotIncreaseSize) {
    Context ctx;
    ctx.set("x", 1);
    ctx.set("x", 2);
    EXPECT_EQ(ctx.size(), 1u);
}
