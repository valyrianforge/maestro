#include <catch2/catch_test_macros.hpp>

#include "maestro/orchestrator/TaskGraph.hpp"

using namespace maestro::orchestrator;

namespace {
Task named(std::string n, int priority = 0) {
    Task t;
    t.name = std::move(n);
    t.priority = priority;
    return t;
}
} // namespace

TEST_CASE("a task with no dependencies is immediately ready", "[graph]") {
    TaskGraph g;
    const auto a = g.addTask(named("a"));
    REQUIRE(g.readyTasks() == std::vector<TaskId>{a});
}

TEST_CASE("a dependent is not ready until its prerequisite succeeds", "[graph]") {
    TaskGraph g;
    const auto a = g.addTask(named("a"));
    const auto b = g.addTask(named("b"));
    g.addDependency(b, a);

    REQUIRE(g.readyTasks() == std::vector<TaskId>{a}); // only a
    g.markRunning(a);
    g.markSucceeded(a, "done");
    REQUIRE(g.readyTasks() == std::vector<TaskId>{b}); // now b unlocks
}

TEST_CASE("ready tasks are ordered by priority then id", "[graph]") {
    TaskGraph g;
    const auto low = g.addTask(named("low", 1));
    const auto high = g.addTask(named("high", 10));
    const auto mid = g.addTask(named("mid", 5));
    REQUIRE(g.readyTasks() == std::vector<TaskId>{high, mid, low});
}

TEST_CASE("a failed dependency blocks its transitive dependents", "[graph]") {
    TaskGraph g;
    const auto a = g.addTask(named("a"));
    const auto b = g.addTask(named("b"));
    const auto c = g.addTask(named("c"));
    g.addDependency(b, a);
    g.addDependency(c, b);

    g.markRunning(a);
    g.markFailed(a);

    REQUIRE(g.at(b).state == TaskState::Blocked);
    REQUIRE(g.at(c).state == TaskState::Blocked); // transitive
    REQUIRE(g.readyTasks().empty());
    REQUIRE(g.isComplete());
}

TEST_CASE("isComplete is false while work remains", "[graph]") {
    TaskGraph g;
    const auto a = g.addTask(named("a"));
    REQUIRE_FALSE(g.isComplete());
    g.markRunning(a);
    g.markSucceeded(a, "");
    REQUIRE(g.isComplete());
}

TEST_CASE("cycle detection catches a back-edge", "[graph]") {
    TaskGraph g;
    const auto a = g.addTask(named("a"));
    const auto b = g.addTask(named("b"));
    g.addDependency(a, b);
    g.addDependency(b, a); // cycle
    REQUIRE(g.hasCycle());
}

TEST_CASE("a DAG diamond has no cycle", "[graph]") {
    TaskGraph g;
    const auto a = g.addTask(named("a"));
    const auto b = g.addTask(named("b"));
    const auto c = g.addTask(named("c"));
    const auto d = g.addTask(named("d"));
    g.addDependency(b, a);
    g.addDependency(c, a);
    g.addDependency(d, b);
    g.addDependency(d, c);
    REQUIRE_FALSE(g.hasCycle());
}

TEST_CASE("stats count terminal states", "[graph]") {
    TaskGraph g;
    const auto a = g.addTask(named("a"));
    const auto b = g.addTask(named("b"));
    g.addDependency(b, a);
    g.markFailed(a); // b becomes blocked

    const auto s = g.stats();
    REQUIRE(s.total == 2);
    REQUIRE(s.failed == 1);
    REQUIRE(s.blocked == 1);
    REQUIRE(s.succeeded == 0);
}
