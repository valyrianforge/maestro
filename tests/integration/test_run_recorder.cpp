// Integration: run a real graph through the Scheduler with a fake executor and
// a RunRecorder attached, then assert the persisted rows reflect the run.
#include <catch2/catch_test_macros.hpp>

#include <functional>

#include "maestro/orchestrator/Orchestrator.hpp"
#include "maestro/runtime/RunRecorder.hpp"
#include "maestro/storage/SqliteStore.hpp"

using namespace maestro::orchestrator;
using maestro::core::ProviderId;
using maestro::runtime::RunRecorder;
using maestro::storage::SqliteStore;

namespace {
class FakeExecutor final : public ITaskExecutor {
public:
    TaskResult execute(const ExecRequest&) override {
        return TaskResult{true, "OUT", std::nullopt, std::nullopt};
    }
};
Task step(std::string name) {
    Task t;
    t.name = std::move(name);
    t.provider = ProviderId{"claude"};
    return t;
}
} // namespace

TEST_CASE("a run through the scheduler is fully persisted", "[integration][storage]") {
    SqliteStore store(":memory:");
    const auto pid = store.createProject("test", "/", SqliteStore::isoNow());
    const auto rid = store.createRun(pid, "chain", "pipeline", SqliteStore::isoNow());

    TaskGraph g;
    const auto a = g.addTask(step("research"));
    const auto b = g.addTask(step("draft"));
    g.addDependency(b, a);

    FakeExecutor exec;
    WorkspaceManager ws;
    AgentManager agents;
    Orchestrator orch(g, exec, ws, agents);

    RunRecorder recorder(store, rid, g);
    orch.setObserver(recorder.observer());
    const RunReport report = orch.run();
    recorder.snapshot(agents);
    store.finishRun(rid, SqliteStore::isoNow(), report.succeeded, report.failed, report.blocked);

    // Tasks persisted with final state + output.
    const auto tasks = store.tasksForRun(rid);
    REQUIRE(tasks.size() == 2);
    REQUIRE(tasks[0].state == "succeeded");
    REQUIRE(tasks[0].output == "OUT");

    // The forwarded-context edge draft<-research is recorded.
    const auto edges = store.edgesForRun(rid);
    REQUIRE(edges.size() == 1);
    REQUIRE(edges[0].dependent == "draft");
    REQUIRE(edges[0].prerequisite == "research");

    // Events captured start+success for both tasks (4 events).
    const auto events = store.eventsForRun(rid);
    REQUIRE(events.size() == 4);
    REQUIRE(events.front().type == "TaskStarted");

    // Run summary updated.
    const auto run = store.getRun(rid);
    REQUIRE(run->succeeded == 2);
    (void)a;
    (void)b;
}
