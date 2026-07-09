#pragma once
#include <functional>
#include <string>
#include <vector>

#include "maestro/orchestrator/AgentManager.hpp"
#include "maestro/orchestrator/Execution.hpp"
#include "maestro/orchestrator/TaskGraph.hpp"
#include "maestro/orchestrator/WorkspaceManager.hpp"

namespace maestro::orchestrator {

// Progress notification emitted as the graph advances (for UI/logging).
struct OrchestratorEvent {
    enum class Type { TaskStarted, TaskSucceeded, TaskFailed, TaskBlocked };
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
    std::vector<TaskId> executionOrder; // tasks actually dispatched, in order
    bool cycleDetected{false};
};

// The brain: drives a TaskGraph to completion. For each ready task it leases an
// agent for that provider, composes the effective prompt by forwarding
// succeeded dependencies' outputs, executes via the injected ITaskExecutor,
// records the result, and stores the output as a workspace artifact. Agents
// never receive upstream results any other way.
class Orchestrator {
public:
    Orchestrator(TaskGraph& graph, ITaskExecutor& executor, WorkspaceManager& workspace,
                 AgentManager& agents);

    void setObserver(std::function<void(const OrchestratorEvent&)> observer);

    // Runs until the graph is complete (or a cycle is detected). Synchronous.
    RunReport run();

    // Key under which a task's output is stored in the workspace.
    [[nodiscard]] static std::string artifactKey(const Task& task);

private:
    [[nodiscard]] std::string composePrompt(const Task& task) const;
    void emitEvent(const OrchestratorEvent& event) const;
    AgentId acquireAgent(const ProviderId& provider, TaskId task);

    TaskGraph& graph_;
    ITaskExecutor& executor_;
    WorkspaceManager& workspace_;
    AgentManager& agents_;
    std::function<void(const OrchestratorEvent&)> observer_;
};

} // namespace maestro::orchestrator
