#pragma once
#include <cstdint>
#include <functional>

#include "maestro/orchestrator/AgentManager.hpp"
#include "maestro/orchestrator/Scheduler.hpp" // OrchestratorEvent
#include "maestro/orchestrator/TaskGraph.hpp"
#include "maestro/storage/SqliteStore.hpp"

namespace maestro::runtime {

// Persists a single orchestration run. Attach observer() to the Orchestrator/
// Scheduler to stream events into the `events` table as they happen, then call
// snapshot() once the run finishes to write the final task states, dependency
// edges (the forwarded-context message flow), and agents.
class RunRecorder {
public:
    RunRecorder(storage::SqliteStore& store, std::int64_t runId,
                const orchestrator::TaskGraph& graph);

    // Observer for Orchestrator::setObserver. Thread-compatible with the
    // scheduler (events are emitted from a single scheduler thread).
    [[nodiscard]] std::function<void(const orchestrator::OrchestratorEvent&)> observer();

    // Writes final tasks, edges, and agents for the run. Call after run().
    void snapshot(const orchestrator::AgentManager& agents);

private:
    storage::SqliteStore& store_;
    std::int64_t runId_;
    const orchestrator::TaskGraph& graph_;
    int seq_{0};
};

} // namespace maestro::runtime
