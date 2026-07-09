#pragma once
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "maestro/orchestrator/AgentManager.hpp"
#include "maestro/orchestrator/Execution.hpp"
#include "maestro/orchestrator/TaskGraph.hpp"
#include "maestro/orchestrator/WorkspaceManager.hpp"

namespace maestro::orchestrator {

// Progress notification emitted as the graph advances (for UI/logging).
struct OrchestratorEvent {
    enum class Type {
        TaskStarted,
        TaskSucceeded,
        TaskFailed,
        TaskBlocked,
        TaskRetrying,
        TaskSpawned, // a subagent/child task was created at runtime (detail = parent name)
    };
    Type type{Type::TaskStarted};
    TaskId task;
    AgentId agent;
    std::string detail;
};

// Summary of a run.
struct RunReport {
    int succeeded{0};
    int failed{0};
    int blocked{0};
    std::vector<TaskId> executionOrder; // tasks dispatched, in dispatch order
    bool cycleDetected{false};
};

struct SchedulerConfig {
    int maxConcurrency{1}; // number of tasks that may run at once
    int maxRetries{0};     // per-task retries on failure before giving up
};

// Concurrent executor of a TaskGraph. Dispatches independent ready tasks in
// parallel up to maxConcurrency, forwarding each dependency's output into the
// dependent's prompt (the orchestrator mediation rule). The graph is mutated
// only on the scheduler thread; worker threads only run the (self-contained)
// executor and report results back through a completion queue.
class Scheduler {
public:
    Scheduler(TaskGraph& graph, ITaskExecutor& executor, WorkspaceManager& workspace,
              AgentManager& agents, SchedulerConfig config = {});

    void setObserver(std::function<void(const OrchestratorEvent&)> observer);

    // Optional hook: when a task succeeds, the expander may inspect its result
    // and return child tasks to add to the live graph. Each child is linked to
    // depend on the completed parent (so the parent's output is forwarded into
    // it), making the graph grow at runtime — i.e. agents spawning subagents.
    using Expander = std::function<std::vector<Task>(const Task& parent, const TaskResult&)>;
    void setExpander(Expander expander);

    // Halt/allow dispatch of new tasks. In-flight tasks always run to completion.
    void pause();
    void resume();

    // Runs until the graph is complete (or a cycle is detected). Blocks.
    RunReport run();

    [[nodiscard]] static std::string artifactKey(const Task& task);

private:
    struct Completion {
        TaskId task;
        AgentId agent;
        TaskResult result;
    };

    void dispatchReady(RunReport& report, int& inFlight, std::vector<std::thread>& workers);
    void launchWorker(TaskId id, AgentId agent, ExecRequest request,
                      std::vector<std::thread>& workers);
    [[nodiscard]] std::string composePrompt(const Task& task) const;
    AgentId acquireAgent(const ProviderId& provider, TaskId task);
    void emitEvent(const OrchestratorEvent& event) const;

    TaskGraph& graph_;
    ITaskExecutor& executor_;
    WorkspaceManager& workspace_;
    AgentManager& agents_;
    SchedulerConfig config_;
    std::function<void(const OrchestratorEvent&)> observer_;
    Expander expander_;

    std::mutex mtx_;
    std::condition_variable cv_;
    std::deque<Completion> completions_;
    std::unordered_map<TaskId, int> retries_;
    bool paused_{false};
};

} // namespace maestro::orchestrator
