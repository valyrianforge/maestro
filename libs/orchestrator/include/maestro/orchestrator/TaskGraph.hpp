#pragma once
#include <string>
#include <unordered_map>
#include <vector>

#include "maestro/orchestrator/Task.hpp"

namespace maestro::orchestrator {

// A directed acyclic graph of tasks. Owns task state and the dependency
// relation. Pure domain logic — no execution, no processes, no threads.
class TaskGraph {
public:
    struct Stats {
        int total{0};
        int succeeded{0};
        int failed{0};
        int blocked{0};
        int pendingOrRunning{0};
    };

    // Adds a task; assigns and returns a fresh id (any incoming id is ignored).
    TaskId addTask(Task task);

    // Records that `dependent` requires `prerequisite` to succeed first.
    void addDependency(TaskId dependent, TaskId prerequisite);

    [[nodiscard]] const Task& at(TaskId id) const;
    [[nodiscard]] Task& at(TaskId id);
    [[nodiscard]] bool contains(TaskId id) const { return tasks_.contains(id); }
    [[nodiscard]] std::size_t size() const noexcept { return tasks_.size(); }

    // Pending tasks whose dependencies have all succeeded, ordered by priority
    // (desc) then id (asc) for deterministic dispatch.
    [[nodiscard]] std::vector<TaskId> readyTasks() const;

    void markRunning(TaskId id);
    void markSucceeded(TaskId id, std::string output);
    // Marks the task Failed and propagates Blocked to every transitive dependent.
    void markFailed(TaskId id);

    // True once no task is Pending or Running.
    [[nodiscard]] bool isComplete() const;
    [[nodiscard]] bool hasCycle() const;
    [[nodiscard]] Stats stats() const;

private:
    void propagateBlocks();

    std::unordered_map<TaskId, Task> tasks_;
    TaskId::value_type nextId_{1};
};

} // namespace maestro::orchestrator
