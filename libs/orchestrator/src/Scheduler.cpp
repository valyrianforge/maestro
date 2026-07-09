#include "maestro/orchestrator/Scheduler.hpp"

#include <utility>

namespace maestro::orchestrator {

Scheduler::Scheduler(TaskGraph& graph, ITaskExecutor& executor, WorkspaceManager& workspace,
                     AgentManager& agents, SchedulerConfig config)
    : graph_(graph),
      executor_(executor),
      workspace_(workspace),
      agents_(agents),
      config_(config) {
    if (config_.maxConcurrency < 1) {
        config_.maxConcurrency = 1;
    }
}

void Scheduler::setObserver(std::function<void(const OrchestratorEvent&)> observer) {
    observer_ = std::move(observer);
}

std::string Scheduler::artifactKey(const Task& task) { return "task/" + task.name + "/output"; }

void Scheduler::emitEvent(const OrchestratorEvent& event) const {
    if (observer_) {
        observer_(event);
    }
}

std::string Scheduler::composePrompt(const Task& task) const {
    std::string prompt = task.prompt;
    for (const TaskId dep : task.dependencies) {
        const Task& depTask = graph_.at(dep);
        if (depTask.state == TaskState::Succeeded) {
            prompt += "\n\n## Context from step \"" + depTask.name + "\":\n" + depTask.output;
        }
    }
    return prompt;
}

AgentId Scheduler::acquireAgent(const ProviderId& provider, TaskId task) {
    AgentId id;
    if (const auto idle = agents_.findIdle(provider)) {
        id = *idle;
    } else {
        id = agents_.createAgent(provider.name + "-agent", provider);
    }
    agents_.setStatus(id, AgentStatus::Busy, task);
    return id;
}

void Scheduler::launchWorker(TaskId id, AgentId agent, ExecRequest request,
                             std::vector<std::thread>& workers) {
    workers.emplace_back([this, id, agent, request = std::move(request)]() {
        TaskResult result = executor_.execute(request);
        {
            std::lock_guard<std::mutex> lock(mtx_);
            completions_.push_back(Completion{id, agent, std::move(result)});
        }
        cv_.notify_all();
    });
}

void Scheduler::dispatchReady(RunReport& report, int& inFlight, std::vector<std::thread>& workers) {
    while (inFlight < config_.maxConcurrency) {
        const std::vector<TaskId> ready = graph_.readyTasks();
        if (ready.empty()) {
            break;
        }
        const TaskId id = ready.front(); // highest priority, deterministic
        const ProviderId provider = graph_.at(id).provider;

        graph_.markRunning(id);
        report.executionOrder.push_back(id);
        const AgentId agent = acquireAgent(provider, id);

        ExecRequest request;
        request.provider = provider;
        request.prompt = composePrompt(graph_.at(id));

        emitEvent(OrchestratorEvent{OrchestratorEvent::Type::TaskStarted, id, agent,
                                    graph_.at(id).name});
        launchWorker(id, agent, std::move(request), workers);
        ++inFlight;
    }
}

RunReport Scheduler::run() {
    RunReport report;
    if (graph_.hasCycle()) {
        report.cycleDetected = true;
        return report;
    }

    std::vector<std::thread> workers;
    int inFlight = 0;

    std::unique_lock<std::mutex> lock(mtx_);
    for (;;) {
        if (!paused_) {
            dispatchReady(report, inFlight, workers);
        }

        if (inFlight == 0) {
            if (paused_) {
                cv_.wait(lock, [&] { return !paused_; });
                continue;
            }
            if (graph_.readyTasks().empty()) {
                break; // complete, or everything remaining is blocked
            }
            continue; // slots free and work ready: loop to dispatch
        }

        cv_.wait(lock, [&] { return !completions_.empty(); });

        while (!completions_.empty()) {
            const Completion c = std::move(completions_.front());
            completions_.pop_front();
            --inFlight;
            agents_.setStatus(c.agent, AgentStatus::Idle, std::nullopt);

            if (c.result.success) {
                graph_.markSucceeded(c.task, c.result.output);
                workspace_.putArtifact(artifactKey(graph_.at(c.task)), c.result.output);
                emitEvent(OrchestratorEvent{OrchestratorEvent::Type::TaskSucceeded, c.task,
                                            c.agent, graph_.at(c.task).name});
            } else if (retries_[c.task] < config_.maxRetries) {
                ++retries_[c.task];
                graph_.at(c.task).state = TaskState::Pending; // requeue for another attempt
                emitEvent(OrchestratorEvent{OrchestratorEvent::Type::TaskRetrying, c.task, c.agent,
                                            graph_.at(c.task).name});
            } else {
                graph_.markFailed(c.task);
                emitEvent(OrchestratorEvent{OrchestratorEvent::Type::TaskFailed, c.task, c.agent,
                                            c.result.output});
            }
        }
    }

    lock.unlock();
    for (std::thread& w : workers) {
        if (w.joinable()) {
            w.join();
        }
    }

    const TaskGraph::Stats stats = graph_.stats();
    report.succeeded = stats.succeeded;
    report.failed = stats.failed;
    report.blocked = stats.blocked;
    return report;
}

void Scheduler::pause() {
    {
        std::lock_guard<std::mutex> lock(mtx_);
        paused_ = true;
    }
    cv_.notify_all();
}

void Scheduler::resume() {
    {
        std::lock_guard<std::mutex> lock(mtx_);
        paused_ = false;
    }
    cv_.notify_all();
}

} // namespace maestro::orchestrator
