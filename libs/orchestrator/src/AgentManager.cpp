#include "maestro/orchestrator/AgentManager.hpp"

#include <stdexcept>

namespace maestro::orchestrator {

AgentId AgentManager::createAgent(std::string name, ProviderId provider) {
    const AgentId id{nextId_++};
    Agent agent;
    agent.id = id;
    agent.name = std::move(name);
    agent.provider = std::move(provider);
    agent.status = AgentStatus::Idle;
    agents_.emplace(id, std::move(agent));
    return id;
}

const Agent& AgentManager::at(AgentId id) const {
    const auto it = agents_.find(id);
    if (it == agents_.end()) {
        throw std::out_of_range("AgentManager::at: unknown agent id");
    }
    return it->second;
}

std::optional<AgentId> AgentManager::findIdle(const ProviderId& provider) const {
    for (const auto& [id, agent] : agents_) {
        if (agent.status == AgentStatus::Idle && agent.provider == provider) {
            return id;
        }
    }
    return std::nullopt;
}

void AgentManager::setStatus(AgentId id, AgentStatus status, std::optional<TaskId> currentTask) {
    const auto it = agents_.find(id);
    if (it == agents_.end()) {
        throw std::out_of_range("AgentManager::setStatus: unknown agent id");
    }
    it->second.status = status;
    it->second.currentTask = currentTask;
}

} // namespace maestro::orchestrator
