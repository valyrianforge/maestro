#include <catch2/catch_test_macros.hpp>

#include <functional>
#include <string>
#include <vector>

#include "maestro/orchestrator/Orchestrator.hpp"

using namespace maestro::orchestrator;
using maestro::core::ProviderId;

namespace {

// Test double: runs a caller-supplied function per request, recording each
// prompt it was asked to execute (so tests can assert on forwarded context).
class FakeExecutor final : public ITaskExecutor {
public:
    explicit FakeExecutor(std::function<TaskResult(const ExecRequest&)> fn)
        : fn_(std::move(fn)) {}

    TaskResult execute(const ExecRequest& request) override {
        prompts.push_back(request.prompt);
        return fn_(request);
    }

    std::vector<std::string> prompts;

private:
    std::function<TaskResult(const ExecRequest&)> fn_;
};

Task step(std::string name, std::string prompt) {
    Task t;
    t.name = std::move(name);
    t.prompt = std::move(prompt);
    t.provider = ProviderId{"claude"};
    return t;
}

TaskResult ok(std::string out) { return TaskResult{true, std::move(out), std::nullopt, std::nullopt}; }

} // namespace

TEST_CASE("orchestrator runs a linear chain in dependency order", "[orchestrator]") {
    TaskGraph g;
    const auto a = g.addTask(step("a", "do a"));
    const auto b = g.addTask(step("b", "do b"));
    g.addDependency(b, a);

    FakeExecutor exec([](const ExecRequest&) { return ok("result"); });
    WorkspaceManager ws;
    AgentManager agents;
    Orchestrator orch(g, exec, ws, agents);

    const RunReport report = orch.run();

    REQUIRE(report.executionOrder == std::vector<TaskId>{a, b});
    REQUIRE(report.succeeded == 2);
    REQUIRE(report.failed == 0);
}

TEST_CASE("orchestrator forwards a dependency's output into the dependent's prompt",
          "[orchestrator]") {
    TaskGraph g;
    g.addTask(step("research", "Research DAGs"));
    const auto draft = g.addTask(step("draft", "Write an intro"));
    g.addDependency(draft, TaskId{1}); // draft depends on research (id 1)

    FakeExecutor exec([](const ExecRequest&) { return ok("FINDINGS"); });
    WorkspaceManager ws;
    AgentManager agents;
    Orchestrator orch(g, exec, ws, agents);
    orch.run();

    // Second prompt executed is the draft, and it must carry research's output.
    REQUIRE(exec.prompts.size() == 2);
    REQUIRE(exec.prompts[1].find("Write an intro") != std::string::npos);
    REQUIRE(exec.prompts[1].find("## Context from step \"research\":") != std::string::npos);
    REQUIRE(exec.prompts[1].find("FINDINGS") != std::string::npos);
}

TEST_CASE("a failed task blocks its dependents, which never execute", "[orchestrator]") {
    TaskGraph g;
    const auto a = g.addTask(step("a", "do a"));
    g.addTask(step("b", "do b"));
    g.addDependency(TaskId{2}, a); // b depends on a

    FakeExecutor exec([](const ExecRequest&) {
        return TaskResult{false, "boom", std::nullopt, std::nullopt};
    });
    WorkspaceManager ws;
    AgentManager agents;
    Orchestrator orch(g, exec, ws, agents);
    const RunReport report = orch.run();

    REQUIRE(report.failed == 1);
    REQUIRE(report.blocked == 1);
    REQUIRE(exec.prompts.size() == 1); // b was never dispatched
}

TEST_CASE("successful task outputs are stored as workspace artifacts", "[orchestrator]") {
    TaskGraph g;
    g.addTask(step("plan", "make a plan"));

    FakeExecutor exec([](const ExecRequest&) { return ok("THE PLAN"); });
    WorkspaceManager ws;
    AgentManager agents;
    Orchestrator orch(g, exec, ws, agents);
    orch.run();

    REQUIRE(ws.getArtifact("task/plan/output") == "THE PLAN");
}

TEST_CASE("agents are released to idle after their task", "[orchestrator]") {
    TaskGraph g;
    g.addTask(step("a", "do a"));

    FakeExecutor exec([](const ExecRequest&) { return ok("x"); });
    WorkspaceManager ws;
    AgentManager agents;
    Orchestrator orch(g, exec, ws, agents);
    orch.run();

    REQUIRE(agents.size() == 1); // one claude agent was created and reused
    for (const auto& [id, agent] : agents.agents()) {
        REQUIRE(agent.status == AgentStatus::Idle);
    }
}

TEST_CASE("a cyclic graph is rejected without executing anything", "[orchestrator]") {
    TaskGraph g;
    const auto a = g.addTask(step("a", "a"));
    const auto b = g.addTask(step("b", "b"));
    g.addDependency(a, b);
    g.addDependency(b, a);

    FakeExecutor exec([](const ExecRequest&) { return ok("x"); });
    WorkspaceManager ws;
    AgentManager agents;
    Orchestrator orch(g, exec, ws, agents);
    const RunReport report = orch.run();

    REQUIRE(report.cycleDetected);
    REQUIRE(exec.prompts.empty());
}

TEST_CASE("emitted events cover start and success", "[orchestrator]") {
    TaskGraph g;
    g.addTask(step("a", "a"));

    FakeExecutor exec([](const ExecRequest&) { return ok("x"); });
    WorkspaceManager ws;
    AgentManager agents;
    Orchestrator orch(g, exec, ws, agents);

    std::vector<OrchestratorEvent::Type> seen;
    orch.setObserver([&](const OrchestratorEvent& e) { seen.push_back(e.type); });
    orch.run();

    REQUIRE(seen == std::vector<OrchestratorEvent::Type>{
                        OrchestratorEvent::Type::TaskStarted,
                        OrchestratorEvent::Type::TaskSucceeded});
}
