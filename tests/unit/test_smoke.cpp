#include <catch2/catch_test_macros.hpp>

#include "maestro/core/Ids.hpp"
#include "maestro/core/ProcessSpec.hpp"

using namespace maestro::core;

TEST_CASE("ProcessSpec can be constructed and holds its fields", "[core]") {
    ProcessSpec spec;
    spec.program = "claude";
    spec.args = {"-p", "hello", "--output-format", "stream-json"};

    REQUIRE(spec.program == "claude");
    REQUIRE(spec.args.size() == 4);
    REQUIRE(spec.inheritParentEnv);
}

TEST_CASE("Strong ids are distinct and hashable", "[core]") {
    const AgentId a{7};
    const TaskId t{7};
    REQUIRE(a.value() == t.value());
    REQUIRE(a.valid());
    REQUIRE_FALSE(AgentId{}.valid());

    std::hash<AgentId> h;
    REQUIRE(h(a) == h(AgentId{7}));
}

TEST_CASE("ProcessExit::succeeded reflects reason and code", "[core]") {
    REQUIRE(ProcessExit{ExitReason::Exited, 0}.succeeded());
    REQUIRE_FALSE(ProcessExit{ExitReason::Exited, 1}.succeeded());
    REQUIRE_FALSE(ProcessExit{ExitReason::Killed, 0}.succeeded());
}
