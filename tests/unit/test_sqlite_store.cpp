#include <catch2/catch_test_macros.hpp>

#include "maestro/storage/SqliteStore.hpp"

using namespace maestro::storage;

namespace {
SqliteStore memStore() { return SqliteStore(":memory:"); }
} // namespace

TEST_CASE("projects and runs round-trip", "[storage]") {
    auto store = memStore();
    const auto pid = store.createProject("demo", "/tmp/demo", "2026-07-10T00:00:00Z");
    REQUIRE(pid > 0);
    const auto rid = store.createRun(pid, "merge sort", "pipeline", "2026-07-10T00:01:00Z");
    REQUIRE(rid > 0);

    const auto run = store.getRun(rid);
    REQUIRE(run.has_value());
    REQUIRE(run->topic == "merge sort");
    REQUIRE(run->mode == "pipeline");
    REQUIRE(run->projectId == pid);
}

TEST_CASE("tasks, edges, and agents persist and read back", "[storage]") {
    auto store = memStore();
    const auto pid = store.createProject("p", "/", "t0");
    const auto rid = store.createRun(pid, "topic", "pipeline", "t1");

    store.saveTask(rid, TaskRecord{0, rid, "research", "claude", "succeeded", 0, "FINDINGS"});
    store.saveTask(rid, TaskRecord{0, rid, "draft", "claude", "succeeded", 0, "DRAFT"});
    store.saveEdge(rid, "draft", "research");
    store.saveAgent(rid, "claude-agent", "claude");

    const auto tasks = store.tasksForRun(rid);
    REQUIRE(tasks.size() == 2);
    REQUIRE(tasks[0].name == "research");
    REQUIRE(tasks[0].output == "FINDINGS");

    const auto edges = store.edgesForRun(rid);
    REQUIRE(edges.size() == 1);
    REQUIRE(edges[0].dependent == "draft");
    REQUIRE(edges[0].prerequisite == "research");

    REQUIRE(store.agentsForRun(rid).size() == 1);
}

TEST_CASE("events preserve sequence order", "[storage]") {
    auto store = memStore();
    const auto pid = store.createProject("p", "/", "t0");
    const auto rid = store.createRun(pid, "topic", "fan", "t1");

    for (int i = 1; i <= 5; ++i) {
        EventRecord e;
        e.seq = i;
        e.ts = "t";
        e.type = "TaskStarted";
        e.taskName = "w" + std::to_string(i);
        e.agentId = i;
        store.appendEvent(rid, e);
    }
    const auto events = store.eventsForRun(rid);
    REQUIRE(events.size() == 5);
    REQUIRE(events.front().seq == 1);
    REQUIRE(events.back().seq == 5);
    REQUIRE(events[2].taskName == "w3");
}

TEST_CASE("finishRun updates counts and listRuns is newest-first", "[storage]") {
    auto store = memStore();
    const auto pid = store.createProject("p", "/", "t0");
    const auto r1 = store.createRun(pid, "first", "pipeline", "t1");
    const auto r2 = store.createRun(pid, "second", "fan", "t2");
    store.finishRun(r1, "t3", 3, 0, 0);

    const auto runs = store.listRuns(10);
    REQUIRE(runs.size() == 2);
    REQUIRE(runs.front().id == r2); // newest first
    const auto first = store.getRun(r1);
    REQUIRE(first->succeeded == 3);
    REQUIRE(first->finishedAt == "t3");
}

TEST_CASE("config is upsert semantics", "[storage]") {
    auto store = memStore();
    REQUIRE_FALSE(store.getConfig("theme").has_value());
    store.setConfig("theme", "dark");
    REQUIRE(store.getConfig("theme") == "dark");
    store.setConfig("theme", "light");
    REQUIRE(store.getConfig("theme") == "light");
}

TEST_CASE("isoNow returns a Z-terminated timestamp", "[storage]") {
    const auto ts = SqliteStore::isoNow();
    REQUIRE(ts.size() == 20);      // YYYY-MM-DDTHH:MM:SSZ
    REQUIRE(ts.back() == 'Z');
}
