#pragma once
#include <optional>
#include <string>

#include "maestro/core/Ids.hpp"
#include "maestro/core/Provider.hpp"

namespace maestro::orchestrator {

using core::AgentId;
using core::ProviderId;
using core::TaskId;

enum class AgentStatus { Idle, Busy, Stopped };

// A lightweight logical actor bound to a provider. Agents hold no channels to
// each other; they only ever run a task the orchestrator assigns. Cheap by
// design so hundreds can exist.
struct Agent {
    AgentId id;
    std::string name;
    ProviderId provider;
    AgentStatus status{AgentStatus::Idle};
    std::optional<TaskId> currentTask;
};

} // namespace maestro::orchestrator
