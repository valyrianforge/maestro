#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

#include "maestro/orchestrator/Scheduler.hpp"

using namespace maestro::orchestrator;
using maestro::core::ProviderId;

namespace {

// Thread-safe fake executor. Tracks the peak number of concurrent executions
// and lets each call's behavior be customized.
class ConcurrentFakeExecutor final : public ITaskExecutor {
public:
    explicit ConcurrentFakeExecutor(std::function<TaskResult(const ExecRequest&)> fn,
                                    std::chrono::milliseconds hold = std::chrono::milliseconds{20})
        : fn_(std::move(fn)), hold_(hold) {}

    TaskResult execute(const ExecRequest& request) override {
        const int now = ++active_;
        updatePeak(now);
        {
            std::lock_guard<std::mutex> lock(mtx_);
            prompts_.push_back(request.prompt);
        }
        std::this_thread::sleep_for(hold_); // force overlap so concurrency is observable
        const TaskResult r = fn_(request);
        --active_;
        return r;
    }

    [[nodiscard]] int peakConcurrency() const { return peak_.load(); }
    [[nodiscard]] std::vector<std::string> prompts() {
        std::lock_guard<std::mutex> lock(mtx_);
        return prompts_;
    }

private:
    void updatePeak(int now) {
        int prev = peak_.load();
        while (now > prev && !peak_.compare_exchange_weak(prev, now)) {
        }
    }

    std::function<TaskResult(const ExecRequest&)> fn_;
    std::chrono::milliseconds hold_;
    std::atomic<int> active_{0};
    std::atomic<int> peak_{0};
    std::mutex mtx_;
    std::vector<std::string> prompts_;
};

Task step(std::string name, int priority = 0) {
    Task t;
    t.name = std::move(name);
    t.prompt = t.name;
    t.provider = ProviderId{"claude"};
    t.priority = priority;
    return t;
}

TaskResult ok(std::string out = "ok") {
    return TaskResult{true, std::move(out), std::nullopt, std::nullopt};
}

} // namespace

TEST_CASE("independent tasks run concurrently up to the pool size", "[scheduler]") {
    TaskGraph g;
    for (int i = 0; i < 6; ++i) {
        g.addTask(step("t" + std::to_string(i)));
    }
    ConcurrentFakeExecutor exec([](const ExecRequest&) { return ok(); });
    WorkspaceManager ws;
    AgentManager agents;
    Scheduler sched(g, exec, ws, agents, SchedulerConfig{/*maxConcurrency=*/3, /*retries=*/0});

    const RunReport report = sched.run();

    REQUIRE(report.succeeded == 6);
    REQUIRE(exec.peakConcurrency() == 3); // exactly the pool size, no more
}

TEST_CASE("dependencies are respected under concurrency", "[scheduler]") {
    TaskGraph g;
    const auto a = g.addTask(step("a"));
    const auto b = g.addTask(step("b"));
    g.addDependency(b, a);

    // If b ever ran alongside a, peak would be 2. It must not.
    ConcurrentFakeExecutor exec([](const ExecRequest&) { return ok(); });
    WorkspaceManager ws;
    AgentManager agents;
    Scheduler sched(g, exec, ws, agents, SchedulerConfig{4, 0});
    const RunReport report = sched.run();

    REQUIRE(report.succeeded == 2);
    REQUIRE(exec.peakConcurrency() == 1); // strictly serialized by the dependency
}

TEST_CASE("a task is retried until it succeeds", "[scheduler]") {
    TaskGraph g;
    g.addTask(step("flaky"));

    std::atomic<int> attempts{0};
    ConcurrentFakeExecutor exec([&](const ExecRequest&) {
        const int n = ++attempts;
        return n < 3 ? TaskResult{false, "fail", std::nullopt, std::nullopt} : ok();
    });
    WorkspaceManager ws;
    AgentManager agents;
    Scheduler sched(g, exec, ws, agents, SchedulerConfig{1, /*maxRetries=*/2});
    const RunReport report = sched.run();

    REQUIRE(attempts.load() == 3); // 1 + 2 retries
    REQUIRE(report.succeeded == 1);
    REQUIRE(report.failed == 0);
}

TEST_CASE("retries are exhausted and dependents are blocked", "[scheduler]") {
    TaskGraph g;
    const auto a = g.addTask(step("a"));
    const auto b = g.addTask(step("b"));
    g.addDependency(b, a);

    ConcurrentFakeExecutor exec(
        [](const ExecRequest&) { return TaskResult{false, "nope", std::nullopt, std::nullopt}; });
    WorkspaceManager ws;
    AgentManager agents;
    Scheduler sched(g, exec, ws, agents, SchedulerConfig{2, /*maxRetries=*/1});
    const RunReport report = sched.run();

    REQUIRE(report.failed == 1);
    REQUIRE(report.blocked == 1);
}

TEST_CASE("maxConcurrency of 1 preserves deterministic priority order", "[scheduler]") {
    TaskGraph g;
    const auto low = g.addTask(step("low", 1));
    const auto high = g.addTask(step("high", 10));
    const auto mid = g.addTask(step("mid", 5));

    ConcurrentFakeExecutor exec([](const ExecRequest&) { return ok(); },
                                std::chrono::milliseconds{0});
    WorkspaceManager ws;
    AgentManager agents;
    Scheduler sched(g, exec, ws, agents, SchedulerConfig{1, 0});
    const RunReport report = sched.run();

    REQUIRE(report.executionOrder == std::vector<TaskId>{high, mid, low});
}

TEST_CASE("forwarded context reaches the dependent under the scheduler", "[scheduler]") {
    TaskGraph g;
    g.addTask(step("research"));
    const auto draft = g.addTask(step("draft"));
    g.addDependency(draft, TaskId{1});

    ConcurrentFakeExecutor exec([](const ExecRequest&) { return ok("FINDINGS"); },
                                std::chrono::milliseconds{0});
    WorkspaceManager ws;
    AgentManager agents;
    Scheduler sched(g, exec, ws, agents, SchedulerConfig{2, 0});
    sched.run();

    auto prompts = exec.prompts();
    REQUIRE(prompts.size() == 2);
    bool draftSawContext = false;
    for (const auto& p : prompts) {
        if (p.find("## Context from step \"research\":") != std::string::npos &&
            p.find("FINDINGS") != std::string::npos) {
            draftSawContext = true;
        }
    }
    REQUIRE(draftSawContext);
}

TEST_CASE("pause halts dispatch and resume finishes the run", "[scheduler]") {
    TaskGraph g;
    for (int i = 0; i < 4; ++i) {
        g.addTask(step("t" + std::to_string(i)));
    }
    ConcurrentFakeExecutor exec([](const ExecRequest&) { return ok(); },
                                std::chrono::milliseconds{5});
    WorkspaceManager ws;
    AgentManager agents;
    Scheduler sched(g, exec, ws, agents, SchedulerConfig{2, 0});

    sched.pause();
    std::thread runner([&] { sched.run(); });
    std::this_thread::sleep_for(std::chrono::milliseconds{40});
    // Paused before running: nothing should have executed yet.
    const bool nothingRanWhilePaused = exec.prompts().empty();
    sched.resume();
    runner.join();

    REQUIRE(nothingRanWhilePaused);
    REQUIRE(g.stats().succeeded == 4);
}
