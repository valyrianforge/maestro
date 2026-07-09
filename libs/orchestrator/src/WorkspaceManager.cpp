#include "maestro/orchestrator/WorkspaceManager.hpp"

namespace maestro::orchestrator {

void WorkspaceManager::putArtifact(std::string key, std::string value) {
    artifacts_[std::move(key)] = std::move(value);
}

std::optional<std::string> WorkspaceManager::getArtifact(std::string_view key) const {
    const auto it = artifacts_.find(std::string{key});
    if (it == artifacts_.end()) {
        return std::nullopt;
    }
    return it->second;
}

bool WorkspaceManager::has(std::string_view key) const {
    return artifacts_.contains(std::string{key});
}

} // namespace maestro::orchestrator
