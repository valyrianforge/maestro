#include <catch2/catch_test_macros.hpp>

#include "maestro/orchestrator/WorkspaceManager.hpp"

using maestro::orchestrator::WorkspaceManager;

TEST_CASE("artifacts can be stored and retrieved", "[workspace]") {
    WorkspaceManager ws;
    ws.putArtifact("task/plan/output", "the plan");
    REQUIRE(ws.has("task/plan/output"));
    REQUIRE(ws.getArtifact("task/plan/output") == "the plan");
    REQUIRE(ws.size() == 1);
}

TEST_CASE("storing the same key overwrites", "[workspace]") {
    WorkspaceManager ws;
    ws.putArtifact("k", "v1");
    ws.putArtifact("k", "v2");
    REQUIRE(ws.getArtifact("k") == "v2");
    REQUIRE(ws.size() == 1);
}

TEST_CASE("absent artifacts return nullopt", "[workspace]") {
    WorkspaceManager ws;
    REQUIRE_FALSE(ws.getArtifact("missing").has_value());
    REQUIRE_FALSE(ws.has("missing"));
}
