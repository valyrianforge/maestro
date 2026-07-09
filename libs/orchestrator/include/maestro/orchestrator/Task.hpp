#pragma once
#include <string>
#include <vector>

#include "maestro/core/Ids.hpp"
#include "maestro/core/Provider.hpp"

namespace maestro::orchestrator {

using core::ProviderId;
using core::TaskId;

enum class TaskState {
    Pending,    // not started; may still be waiting on dependencies
    Running,    // dispatched to an executor
    Succeeded,  // completed successfully
    Failed,     // ran but failed
    Blocked,    // a dependency failed/was blocked; can never run
};

[[nodiscard]] inline bool isTerminal(TaskState s) noexcept {
    return s == TaskState::Succeeded || s == TaskState::Failed || s == TaskState::Blocked;
}

// One node in the task graph. Carries what to run (prompt + provider), how it
// relates to other work (dependencies, priority), and its live state/output.
struct Task {
    TaskId id;
    std::string name;
    std::string prompt;
    ProviderId provider;
    std::vector<TaskId> dependencies;
    int priority{0};                       // higher runs first among ready tasks
    TaskState state{TaskState::Pending};
    std::string output;                    // populated on success
};

} // namespace maestro::orchestrator
