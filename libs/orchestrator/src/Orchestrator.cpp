#include "maestro/orchestrator/Orchestrator.hpp"

namespace maestro::orchestrator {

Orchestrator::Orchestrator(TaskGraph& graph, ITaskExecutor& executor, WorkspaceManager& workspace,
                           AgentManager& agents)
    : graph_(graph), executor_(executor), workspace_(workspace), agents_(agents) {}

void Orchestrator::setObserver(std::function<void(const OrchestratorEvent&)> observer) {
    observer_ = std::move(observer);
}

std::string Orchestrator::artifactKey(const Task& task) {
    return "task/" + task.name + "/output";
}

void Orchestrator::emitEvent(const OrchestratorEvent& event) const {
    if (observer_) {
        observer_(event);
    }
}

std::string Orchestrator::composePrompt(const Task& task) const {
    std::string prompt = task.prompt;
    for (const TaskId dep : task.dependencies) {
        const Task& depTask = graph_.at(dep);
        if (depTask.state == TaskState::Succeeded) {
            prompt += "\n\n## Context from step \"" + depTask.name + "\":\n" + depTask.output;
        }
    }
    return prompt;
}

AgentId Orchestrator::acquireAgent(const ProviderId& provider, TaskId task) {
    AgentId id;
    if (const auto idle = agents_.findIdle(provider)) {
        id = *idle;
    } else {
        id = agents_.createAgent(provider.name + "-agent", provider);
    }
    agents_.setStatus(id, AgentStatus::Busy, task);
    return id;
}

RunReport Orchestrator::run() {
    RunReport report;
    if (graph_.hasCycle()) {
        report.cycleDetected = true;
        return report;
    }

    for (;;) {
        const std::vector<TaskId> ready = graph_.readyTasks();
        if (ready.empty()) {
            break; // nothing runnable: either complete or everything else is blocked
        }

        for (const TaskId id : ready) {
            const Task& task = graph_.at(id);
            const ProviderId provider = task.provider;
            const std::string name = task.name;

            const AgentId agent = acquireAgent(provider, id);
            graph_.markRunning(id);
            report.executionOrder.push_back(id);
            emitEvent(OrchestratorEvent{OrchestratorEvent::Type::TaskStarted, id, agent, name});

            ExecRequest request;
            request.provider = provider;
            request.prompt = composePrompt(graph_.at(id));

            const TaskResult result = executor_.execute(request);

            if (result.success) {
                graph_.markSucceeded(id, result.output);
                workspace_.putArtifact(artifactKey(graph_.at(id)), result.output);
                emitEvent(OrchestratorEvent{OrchestratorEvent::Type::TaskSucceeded, id, agent, name});
            } else {
                graph_.markFailed(id);
                emitEvent(OrchestratorEvent{OrchestratorEvent::Type::TaskFailed, id, agent,
                                       result.output});
            }
            agents_.setStatus(agent, AgentStatus::Idle, std::nullopt);
        }
    }

    const TaskGraph::Stats stats = graph_.stats();
    report.succeeded = stats.succeeded;
    report.failed = stats.failed;
    report.blocked = stats.blocked;
    return report;
}

} // namespace maestro::orchestrator
