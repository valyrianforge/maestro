#include "maestro/orchestrator/TaskGraph.hpp"

#include <algorithm>
#include <stdexcept>
#include <unordered_set>

namespace maestro::orchestrator {

TaskId TaskGraph::addTask(Task task) {
    const TaskId id{nextId_++};
    task.id = id;
    tasks_.emplace(id, std::move(task));
    return id;
}

void TaskGraph::addDependency(TaskId dependent, TaskId prerequisite) {
    at(dependent).dependencies.push_back(prerequisite);
}

const Task& TaskGraph::at(TaskId id) const {
    const auto it = tasks_.find(id);
    if (it == tasks_.end()) {
        throw std::out_of_range("TaskGraph::at: unknown task id");
    }
    return it->second;
}

Task& TaskGraph::at(TaskId id) {
    const auto it = tasks_.find(id);
    if (it == tasks_.end()) {
        throw std::out_of_range("TaskGraph::at: unknown task id");
    }
    return it->second;
}

std::vector<TaskId> TaskGraph::readyTasks() const {
    std::vector<TaskId> ready;
    for (const auto& [id, task] : tasks_) {
        if (task.state != TaskState::Pending) {
            continue;
        }
        const bool depsSatisfied =
            std::all_of(task.dependencies.begin(), task.dependencies.end(),
                        [&](TaskId dep) { return at(dep).state == TaskState::Succeeded; });
        if (depsSatisfied) {
            ready.push_back(id);
        }
    }
    std::sort(ready.begin(), ready.end(), [&](TaskId a, TaskId b) {
        const int pa = at(a).priority;
        const int pb = at(b).priority;
        if (pa != pb) {
            return pa > pb; // higher priority first
        }
        return a.value() < b.value(); // stable, deterministic
    });
    return ready;
}

void TaskGraph::markRunning(TaskId id) { at(id).state = TaskState::Running; }

void TaskGraph::markSucceeded(TaskId id, std::string output) {
    Task& task = at(id);
    task.state = TaskState::Succeeded;
    task.output = std::move(output);
}

void TaskGraph::markFailed(TaskId id) {
    at(id).state = TaskState::Failed;
    propagateBlocks();
}

void TaskGraph::propagateBlocks() {
    // Fixpoint: a Pending task whose dependency is Failed or Blocked becomes
    // Blocked, which can in turn block its own dependents.
    bool changed = true;
    while (changed) {
        changed = false;
        for (auto& [id, task] : tasks_) {
            if (task.state != TaskState::Pending) {
                continue;
            }
            const bool depDead =
                std::any_of(task.dependencies.begin(), task.dependencies.end(), [&](TaskId dep) {
                    const TaskState s = at(dep).state;
                    return s == TaskState::Failed || s == TaskState::Blocked;
                });
            if (depDead) {
                task.state = TaskState::Blocked;
                changed = true;
            }
        }
    }
}

bool TaskGraph::isComplete() const {
    return std::none_of(tasks_.begin(), tasks_.end(), [](const auto& kv) {
        return kv.second.state == TaskState::Pending || kv.second.state == TaskState::Running;
    });
}

bool TaskGraph::hasCycle() const {
    enum class Mark { Unseen, InProgress, Done };
    std::unordered_map<TaskId, Mark> marks;
    for (const auto& [id, task] : tasks_) {
        marks[id] = Mark::Unseen;
    }

    // Iterative DFS over the dependency edges.
    for (const auto& [start, task] : tasks_) {
        if (marks[start] != Mark::Unseen) {
            continue;
        }
        std::vector<std::pair<TaskId, std::size_t>> stack;
        stack.push_back({start, 0});
        marks[start] = Mark::InProgress;
        while (!stack.empty()) {
            auto& [node, idx] = stack.back();
            const Task& n = at(node);
            if (idx < n.dependencies.size()) {
                const TaskId dep = n.dependencies[idx];
                ++idx;
                if (marks[dep] == Mark::InProgress) {
                    return true; // back-edge
                }
                if (marks[dep] == Mark::Unseen) {
                    marks[dep] = Mark::InProgress;
                    stack.push_back({dep, 0});
                }
            } else {
                marks[node] = Mark::Done;
                stack.pop_back();
            }
        }
    }
    return false;
}

TaskGraph::Stats TaskGraph::stats() const {
    Stats s;
    s.total = static_cast<int>(tasks_.size());
    for (const auto& [id, task] : tasks_) {
        switch (task.state) {
        case TaskState::Succeeded: ++s.succeeded; break;
        case TaskState::Failed:    ++s.failed; break;
        case TaskState::Blocked:   ++s.blocked; break;
        case TaskState::Pending:
        case TaskState::Running:   ++s.pendingOrRunning; break;
        }
    }
    return s;
}

} // namespace maestro::orchestrator
