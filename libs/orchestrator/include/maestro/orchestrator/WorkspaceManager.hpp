#pragma once
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace maestro::orchestrator {

// The shared project state agents work against instead of talking directly.
// M3 keeps artifacts in memory as a key -> value store; persistence (SQLite)
// arrives in M5 behind this same surface.
class WorkspaceManager {
public:
    void putArtifact(std::string key, std::string value);
    [[nodiscard]] std::optional<std::string> getArtifact(std::string_view key) const;
    [[nodiscard]] bool has(std::string_view key) const;
    [[nodiscard]] std::size_t size() const noexcept { return artifacts_.size(); }

private:
    std::unordered_map<std::string, std::string> artifacts_;
};

} // namespace maestro::orchestrator
