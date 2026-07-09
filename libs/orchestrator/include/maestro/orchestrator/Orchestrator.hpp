#pragma once
#include <functional>
#include <string>

#include "maestro/orchestrator/Scheduler.hpp"

namespace maestro::orchestrator {

// The brain: turns work into a driven TaskGraph. It is a thin facade over
// Scheduler — the actual concurrent dispatch, agent leasing, context
// forwarding, and result storage live there. Default concurrency is 1
// (sequential, deterministic); pass a SchedulerConfig for parallelism.
//
// OrchestratorEvent and RunReport are declared in Scheduler.hpp.
class Orchestrator {
public:
    Orchestrator(TaskGraph& graph, ITaskExecutor& executor, WorkspaceManager& workspace,
                 AgentManager& agents, SchedulerConfig config = {})
        : scheduler_(graph, executor, workspace, agents, config) {}

    void setObserver(std::function<void(const OrchestratorEvent&)> observer) {
        scheduler_.setObserver(std::move(observer));
    }

    RunReport run() { return scheduler_.run(); }

    void pause() { scheduler_.pause(); }
    void resume() { scheduler_.resume(); }

    [[nodiscard]] static std::string artifactKey(const Task& task) {
        return Scheduler::artifactKey(task);
    }

private:
    Scheduler scheduler_;
};

} // namespace maestro::orchestrator
