#include <catch2/catch_test_macros.hpp>

#include "maestro/orchestrator/AgentManager.hpp"

using namespace maestro::orchestrator;
using maestro::core::ProviderId;

TEST_CASE("created agents start idle", "[agent]") {
    AgentManager m;
    const auto id = m.createAgent("claude-1", ProviderId{"claude"});
    REQUIRE(m.at(id).status == AgentStatus::Idle);
    REQUIRE(m.at(id).name == "claude-1");
    REQUIRE(m.size() == 1);
}

TEST_CASE("findIdle matches on provider and skips busy agents", "[agent]") {
    AgentManager m;
    const auto claude = m.createAgent("c", ProviderId{"claude"});
    m.createAgent("x", ProviderId{"codex"});

    REQUIRE(m.findIdle(ProviderId{"claude"}) == claude);
    REQUIRE_FALSE(m.findIdle(ProviderId{"nonexistent"}).has_value());

    m.setStatus(claude, AgentStatus::Busy, maestro::core::TaskId{42});
    REQUIRE_FALSE(m.findIdle(ProviderId{"claude"}).has_value()); // now busy
    REQUIRE(m.at(claude).currentTask == maestro::core::TaskId{42});
}

TEST_CASE("status can return to idle and clears the current task", "[agent]") {
    AgentManager m;
    const auto id = m.createAgent("c", ProviderId{"claude"});
    m.setStatus(id, AgentStatus::Busy, maestro::core::TaskId{1});
    m.setStatus(id, AgentStatus::Idle);
    REQUIRE(m.at(id).status == AgentStatus::Idle);
    REQUIRE_FALSE(m.at(id).currentTask.has_value());
}
