#include "maestro/runtime/RunRecorder.hpp"

namespace maestro::runtime {

namespace orch = maestro::orchestrator;

namespace {

const char* eventTypeName(orch::OrchestratorEvent::Type t) {
    switch (t) {
    case orch::OrchestratorEvent::Type::TaskStarted:   return "TaskStarted";
    case orch::OrchestratorEvent::Type::TaskSucceeded: return "TaskSucceeded";
    case orch::OrchestratorEvent::Type::TaskFailed:    return "TaskFailed";
    case orch::OrchestratorEvent::Type::TaskBlocked:   return "TaskBlocked";
    case orch::OrchestratorEvent::Type::TaskRetrying:  return "TaskRetrying";
    }
    return "Unknown";
}

const char* taskStateName(orch::TaskState s) {
    switch (s) {
    case orch::TaskState::Pending:   return "pending";
    case orch::TaskState::Running:   return "running";
    case orch::TaskState::Succeeded: return "succeeded";
    case orch::TaskState::Failed:    return "failed";
    case orch::TaskState::Blocked:   return "blocked";
    }
    return "unknown";
}

} // namespace

RunRecorder::RunRecorder(storage::SqliteStore& store, std::int64_t runId,
                         const orch::TaskGraph& graph)
    : store_(store), runId_(runId), graph_(graph) {}

std::function<void(const orch::OrchestratorEvent&)> RunRecorder::observer() {
    return [this](const orch::OrchestratorEvent& e) {
        storage::EventRecord rec;
        rec.seq = ++seq_;
        rec.ts = storage::SqliteStore::isoNow();
        rec.type = eventTypeName(e.type);
        rec.taskName = graph_.contains(e.task) ? graph_.at(e.task).name : std::string{};
        rec.agentId = static_cast<std::int64_t>(e.agent.value());
        rec.detail = e.detail;
        store_.appendEvent(runId_, rec);
    };
}

void RunRecorder::snapshot(const orch::AgentManager& agents) {
    // Tasks are assigned ids 1..N by TaskGraph::addTask.
    for (std::size_t i = 1; i <= graph_.size(); ++i) {
        const orch::TaskId id{i};
        if (!graph_.contains(id)) {
            continue;
        }
        const orch::Task& task = graph_.at(id);

        storage::TaskRecord rec;
        rec.name = task.name;
        rec.provider = task.provider.name;
        rec.state = taskStateName(task.state);
        rec.priority = task.priority;
        rec.output = task.output;
        store_.saveTask(runId_, rec);

        for (const orch::TaskId dep : task.dependencies) {
            if (graph_.contains(dep)) {
                store_.saveEdge(runId_, task.name, graph_.at(dep).name);
            }
        }
    }

    for (const auto& [id, agent] : agents.agents()) {
        store_.saveAgent(runId_, agent.name, agent.provider.name);
    }
}

} // namespace maestro::runtime
