#include <gtest/gtest.h>

#include "common/dependency_graph.h"

using namespace math_solver;

TEST(DependencyGraph, AddAndQuery) {
    DependencyGraph g;
    g.add_variable("y", {"x"});
    g.add_variable("z", {"y", "x"});

    auto deps_z = g.dependencies_of("z");
    EXPECT_EQ(deps_z.size(), 2u);
    EXPECT_TRUE(deps_z.count("y"));
    EXPECT_TRUE(deps_z.count("x"));

    auto deps_y = g.dependencies_of("y");
    EXPECT_EQ(deps_y.size(), 1u);
    EXPECT_TRUE(deps_y.count("x"));
}

TEST(DependencyGraph, WouldCycleDetects) {
    DependencyGraph g;
    g.add_variable("y", {"x"});

    // Setting x = f(y) would create a cycle: x→y→x
    EXPECT_TRUE(g.would_cycle("x", {"y"}));
    // Setting z = f(x) is fine
    EXPECT_FALSE(g.would_cycle("z", {"x"}));
}

TEST(DependencyGraph, DependentsOf) {
    DependencyGraph g;
    g.add_variable("y", {"x"});
    g.add_variable("z", {"y"});

    auto deps_x = g.dependents_of("x");
    EXPECT_EQ(deps_x.size(), 1u);
    EXPECT_TRUE(deps_x.count("y"));

    auto deps_y = g.dependents_of("y");
    EXPECT_EQ(deps_y.size(), 1u);
    EXPECT_TRUE(deps_y.count("z"));
}

TEST(DependencyGraph, TransitiveDeps) {
    DependencyGraph g;
    g.add_variable("y", {"x"});
    g.add_variable("z", {"y"});
    g.add_variable("w", {"z"});

    // w depends on z, z depends on y, y depends on x
    // Transitive deps of w should be {z, y, x}
    auto transitive = g.transitive_deps("w");
    EXPECT_EQ(transitive.size(), 3u);
    EXPECT_TRUE(transitive.count("z"));
    EXPECT_TRUE(transitive.count("y"));
    EXPECT_TRUE(transitive.count("x"));

    // Transitive deps of x should be empty (leaf node)
    auto leaf_deps = g.transitive_deps("x");
    EXPECT_TRUE(leaf_deps.empty());
}

TEST(DependencyGraph, RemoveVariable) {
    DependencyGraph g;
    g.add_variable("y", {"x"});
    g.add_variable("z", {"y"});

    g.remove_variable("y");

    // y no longer depends on x
    EXPECT_TRUE(g.dependencies_of("y").empty());
    // x no longer has y as dependent
    EXPECT_TRUE(g.dependents_of("x").empty());
    // z still depends on y (but y has no forward edges)
    auto deps_z = g.dependencies_of("z");
    EXPECT_TRUE(deps_z.count("y"));
}

TEST(DependencyGraph, Clear) {
    DependencyGraph g;
    g.add_variable("y", {"x"});
    g.add_variable("z", {"y"});
    g.clear();

    EXPECT_TRUE(g.dependencies_of("y").empty());
    EXPECT_TRUE(g.dependents_of("x").empty());
    EXPECT_FALSE(g.would_cycle("x", {"y"}));
}

TEST(DependencyGraph, TopologicalOrder) {
    DependencyGraph g;
    g.add_variable("y", {"x"});
    g.add_variable("z", {"y", "x"});
    g.add_variable("w", {"z"});

    auto order = g.topological_order();
    // x must come before y, y before z, z before w
    // Find positions
    auto pos   = [&](const std::string& s) -> int {
        for (int i = 0; i < (int)order.size(); ++i)
            if (order[i] == s)
                return i;
        return -1;
    };

    // x has no forward edges so may not appear; y, z, w should be ordered
    if (pos("x") >= 0) {
        EXPECT_LT(pos("x"), pos("y"));
    }
    EXPECT_LT(pos("y"), pos("z"));
    EXPECT_LT(pos("z"), pos("w"));
}
