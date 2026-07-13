#include <catch2/catch_test_macros.hpp>

#include <filesystem>

#include "maestro/storage/SqliteStore.hpp"

using namespace maestro::storage;

TEST_CASE("sessions persist and expose the nucleus->electron structure", "[storage][v2]") {
    SqliteStore store(":memory:");

    store.upsertSession({"nucleus-1", "nucleus", "", "idle", "2026-07-12T00:00:00Z"});
    store.upsertSession({"worker-a", "worker", "nucleus-1", "busy", "2026-07-12T00:00:01Z"});
    store.upsertSession({"worker-b", "worker", "nucleus-1", "busy", "2026-07-12T00:00:02Z"});

    REQUIRE(store.listSessions().size() == 3);

    const auto kids = store.childSessions("nucleus-1");
    REQUIRE(kids.size() == 2);
    REQUIRE(kids[0].sessionId == "worker-a");

    // upsert updates in place; state change persists.
    store.updateSessionState("worker-a", "done");
    const auto again = store.childSessions("nucleus-1");
    REQUIRE(again[0].state == "done");
    REQUIRE(store.listSessions().size() == 3); // no duplicate row

    store.removeSession("worker-b");
    REQUIRE(store.childSessions("nucleus-1").size() == 1);
}

TEST_CASE("plans save with status and round-trip their workers payload", "[storage][v2]") {
    SqliteStore store(":memory:");

    const std::int64_t id = store.savePlan(
        {0, "nucleus-1", "proposed", R"([{"role":"auth","goal":"login"}])", "2026-07-12T00:00:00Z"});
    REQUIRE(id > 0);

    auto p = store.getPlan(id);
    REQUIRE(p.has_value());
    REQUIRE(p->status == "proposed");
    REQUIRE(p->workersJson.find("auth") != std::string::npos);

    store.setPlanStatus(id, "approved");
    REQUIRE(store.getPlan(id)->status == "approved");
}

TEST_CASE("the decision log is append-only and queryable per session", "[storage][v2]") {
    SqliteStore store(":memory:");

    store.appendDecision({0, "worker-a", "t1", "tool_allowed", "Read auth.cpp"});
    store.appendDecision({0, "worker-a", "t2", "tool_denied", "rm -rf"});
    store.appendDecision({0, "", "t3", "plan_approved", "3 workers"});

    const auto forA = store.decisionsForSession("worker-a");
    REQUIRE(forA.size() == 2);
    REQUIRE(forA[0].kind == "tool_allowed"); // ascending by id
    REQUIRE(forA[1].kind == "tool_denied");

    const auto recent = store.recentDecisions(10);
    REQUIRE(recent.size() == 3);
    REQUIRE(recent[0].kind == "plan_approved"); // newest first
}

TEST_CASE("v2 state survives a store reopen (restart)", "[storage][v2]") {
    namespace fs = std::filesystem;
    const fs::path dbPath = fs::temp_directory_path() / "maestro_store_v2_reopen.db";
    fs::remove(dbPath);

    {
        SqliteStore store(dbPath.string());
        store.upsertSession({"nucleus-1", "nucleus", "", "busy", "2026-07-12T00:00:00Z"});
        store.savePlan({0, "nucleus-1", "approved", "[]", "2026-07-12T00:00:00Z"});
    } // store closed

    {
        SqliteStore reopened(dbPath.string());
        REQUIRE(reopened.listSessions().size() == 1);
        REQUIRE(reopened.getPlan(1).has_value());
        REQUIRE(reopened.getPlan(1)->status == "approved");
    }

    fs::remove(dbPath);
}
