#pragma once
#include <optional>
#include <string>
#include <unordered_map>

#include "maestro/orchestrator/Agent.hpp"

namespace maestro::orchestrator {

// Owns the set of agents. Provides creation, lookup, and the idle-agent query
// the orchestrator uses to lease an actor for a task.
class AgentManager {
public:
    AgentId createAgent(std::string name, ProviderId provider);

    [[nodiscard]] const Agent& at(AgentId id) const;
    [[nodiscard]] bool contains(AgentId id) const { return agents_.contains(id); }
    [[nodiscard]] std::size_t size() const noexcept { return agents_.size(); }

    // First idle agent bound to `provider`, if any.
    [[nodiscard]] std::optional<AgentId> findIdle(const ProviderId& provider) const;

    void setStatus(AgentId id, AgentStatus status, std::optional<TaskId> currentTask = std::nullopt);

    [[nodiscard]] const std::unordered_map<AgentId, Agent>& agents() const noexcept {
        return agents_;
    }

private:
    std::unordered_map<AgentId, Agent> agents_;
    AgentId::value_type nextId_{1};
};

} // namespace maestro::orchestrator
