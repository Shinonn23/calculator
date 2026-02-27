#include <gtest/gtest.h>

#include "ast/command/history_entry.hpp"

using namespace math_solver;

// ── to_string ────────────────────────────────────────────────────────────

TEST(HistoryStatusTest, ToStringAllVariants) {
    EXPECT_EQ(to_string(HistoryStatus::Success), "success");
    EXPECT_EQ(to_string(HistoryStatus::Error),   "error");
    EXPECT_EQ(to_string(HistoryStatus::Warning), "warning");
    EXPECT_EQ(to_string(HistoryStatus::Info),    "info");
    EXPECT_EQ(to_string(HistoryStatus::Unknown), "unknown");
}

// ── parse_history_status ────────────────────────────────────────────────

TEST(HistoryStatusTest, ParseKnownStrings) {
    EXPECT_EQ(parse_history_status("success"), HistoryStatus::Success);
    EXPECT_EQ(parse_history_status("error"),   HistoryStatus::Error);
    EXPECT_EQ(parse_history_status("warning"), HistoryStatus::Warning);
    EXPECT_EQ(parse_history_status("info"),    HistoryStatus::Info);
}

TEST(HistoryStatusTest, ParseUnknownStringReturnsUnknown) {
    EXPECT_EQ(parse_history_status("garbage"),   HistoryStatus::Unknown);
    EXPECT_EQ(parse_history_status(""),          HistoryStatus::Unknown);
    EXPECT_EQ(parse_history_status("SUCCESS"),   HistoryStatus::Unknown);
}

TEST(HistoryStatusTest, Roundtrip) {
    for (auto s : {HistoryStatus::Success, HistoryStatus::Error,
                   HistoryStatus::Warning, HistoryStatus::Info}) {
        EXPECT_EQ(parse_history_status(to_string(s)), s);
    }
}

// ── HistoryEntry predicates ──────────────────────────────────────────────

TEST(HistoryEntryTest, SuccessPredicates) {
    HistoryEntry e{"cmd", HistoryStatus::Success, ""};
    EXPECT_TRUE(e.is_success());
    EXPECT_FALSE(e.is_error());
    EXPECT_FALSE(e.is_warning());
    EXPECT_FALSE(e.is_info());
    EXPECT_FALSE(e.failed());
}

TEST(HistoryEntryTest, ErrorPredicates) {
    HistoryEntry e{"cmd", HistoryStatus::Error, ""};
    EXPECT_FALSE(e.is_success());
    EXPECT_TRUE(e.is_error());
    EXPECT_TRUE(e.failed());
}

TEST(HistoryEntryTest, WarningPredicates) {
    HistoryEntry e{"cmd", HistoryStatus::Warning, ""};
    EXPECT_TRUE(e.is_warning());
    EXPECT_FALSE(e.is_success());
    EXPECT_FALSE(e.failed());
}

TEST(HistoryEntryTest, InfoPredicates) {
    HistoryEntry e{"cmd", HistoryStatus::Info, ""};
    EXPECT_TRUE(e.is_info());
    EXPECT_FALSE(e.is_success());
    EXPECT_FALSE(e.failed());
}

TEST(HistoryEntryTest, OnlyOnePredicateIsTrueAtATime) {
    auto count_true = [](const HistoryEntry& e) {
        return (int)e.is_success() + (int)e.is_error() +
               (int)e.is_warning() + (int)e.is_info();
    };
    EXPECT_EQ(count_true({"", HistoryStatus::Success, ""}), 1);
    EXPECT_EQ(count_true({"", HistoryStatus::Error,   ""}), 1);
    EXPECT_EQ(count_true({"", HistoryStatus::Warning, ""}), 1);
    EXPECT_EQ(count_true({"", HistoryStatus::Info,    ""}), 1);
}
