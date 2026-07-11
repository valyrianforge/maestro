#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "maestro/acp/AcpClient.hpp"
#include "maestro/acp/FakeAcpTransport.hpp"
#include "maestro/acp/Nucleus.hpp"
#include "maestro/acp/Plan.hpp"
#include "maestro/acp/PlanApprovalGate.hpp"
#include "maestro/acp/SessionManager.hpp"

using namespace maestro::acp;

namespace {

// A propose_plan tool-call update carrying two workers.
SessionUpdate proposePlanUpdate() {
    SessionUpdate u;
    u.kind = SessionUpdate::Kind::ToolCall;
    u.sessionId = "nucleus-1";
    u.data = Json{
        {"title", "propose_plan"},
        {"rawInput",
         {{"workers",
           Json::array({{{"role", "auth"}, {"goal", "implement login"}, {"files", {"auth.cpp"}}},
                        {{"role", "tests"}, {"goal", "write auth tests"}}})}}}};
    return u;
}

} // namespace

TEST_CASE("parsePlanProposal reads worker specs, tolerating missing fields", "[nucleus]") {
    const Json input = {
        {"workers", Json::array({{{"role", "a"}, {"goal", "do a"}, {"files", {"x.cpp", "y.cpp"}}},
                                 {{"role", "b"}} /* no goal/files */})}};
    const Plan plan = parsePlanProposal(input);

    REQUIRE(plan.size() == 2);
    REQUIRE(plan.workers[0].role == "a");
    REQUIRE(plan.workers[0].files == std::vector<std::string>{"x.cpp", "y.cpp"});
    REQUIRE(plan.workers[1].role == "b");
    REQUIRE(plan.workers[1].goal.empty());
}

TEST_CASE("the plan gate holds a proposal until approved or rejected", "[nucleus]") {
    PlanApprovalGate gate;

    Plan seen;
    int proposed = 0, approved = 0, rejected = 0;
    gate.onPlanProposed = [&](const Plan& p) { seen = p; ++proposed; };
    gate.onApproved = [&](const Plan&) { ++approved; };
    gate.onRejected = [&] { ++rejected; };

    Plan p;
    p.workers.push_back({"a", "goal", {}});
    gate.propose(p);

    REQUIRE(proposed == 1);
    REQUIRE(gate.hasPending());
    REQUIRE(seen.size() == 1);

    REQUIRE(gate.approve());
    REQUIRE(approved == 1);
    REQUIRE_FALSE(gate.hasPending());
    REQUIRE_FALSE(gate.approve()); // nothing pending now
    REQUIRE_FALSE(gate.reject());
    REQUIRE(rejected == 0);
}

TEST_CASE("approving with edits launches the edited plan", "[nucleus]") {
    PlanApprovalGate gate;
    Plan finalPlan;
    gate.onApproved = [&](const Plan& p) { finalPlan = p; };

    Plan proposed;
    proposed.workers.push_back({"a", "original", {}});
    gate.propose(proposed);

    Plan edited;
    edited.workers.push_back({"a", "edited goal", {}});
    edited.workers.push_back({"b", "added worker", {}});
    REQUIRE(gate.approve(edited));

    REQUIRE(finalPlan.size() == 2);
    REQUIRE(finalPlan.workers[0].goal == "edited goal");
}

TEST_CASE("nucleus routes propose_plan through the gate and spawns on approval", "[nucleus]") {
    FakeAcpTransport transport;
    AcpClient client(transport);
    SessionManager mgr;

    // Spawner registers each worker under the nucleus and returns its controller.
    int nextIdx = 0;
    Nucleus::Spawner spawner = [&](const WorkerSpec& spec) -> SessionController* {
        const std::string id = "worker-" + std::to_string(nextIdx++);
        return &mgr.add({id, spec.role, std::string("nucleus-1")}, client);
    };

    Nucleus nucleus("nucleus-1", spawner);

    int proposed = 0;
    nucleus.gate().onPlanProposed = [&](const Plan& p) { proposed = static_cast<int>(p.size()); };

    // Nucleus emits a propose_plan tool call -> gate holds it, nothing spawned yet.
    nucleus.onNucleusUpdate(proposePlanUpdate());
    REQUIRE(proposed == 2);
    REQUIRE(nucleus.gate().hasPending());
    REQUIRE(mgr.size() == 0);

    // Human approves -> both electrons spawn and each receives its goal (a turn).
    REQUIRE(nucleus.gate().approve());
    REQUIRE(nucleus.workerIds().size() == 2);
    REQUIRE(mgr.size() == 2);
    REQUIRE(mgr.childrenOf("nucleus-1").size() == 2);

    SessionController* w0 = mgr.controller(nucleus.workerIds()[0]);
    REQUIRE(w0 != nullptr);
    REQUIRE(w0->state() == SessionController::State::Busy); // goal dispatched
}

TEST_CASE("non-propose_plan updates are ignored by the nucleus", "[nucleus]") {
    Nucleus nucleus("nucleus-1", [](const WorkerSpec&) -> SessionController* { return nullptr; });

    SessionUpdate chatter;
    chatter.kind = SessionUpdate::Kind::AgentMessageChunk;
    chatter.data = Json{{"content", {{"text", "thinking..."}}}};

    nucleus.onNucleusUpdate(chatter);
    REQUIRE_FALSE(nucleus.gate().hasPending());
}
