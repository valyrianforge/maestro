#pragma once
#include <optional>
#include <string>

#include "maestro/core/Provider.hpp"
#include "maestro/core/SessionId.hpp"

namespace maestro::orchestrator {

using core::ProviderId;
using core::SessionId;

// A self-contained request to run one prompt against one provider. The
// orchestrator builds this (composing any forwarded context into `prompt`); the
// executor knows nothing about graphs or agents.
struct ExecRequest {
    ProviderId provider;
    std::string prompt;
    std::optional<SessionId> resume;
    std::optional<std::string> workingDirectory;
};

// The outcome of running an ExecRequest.
struct TaskResult {
    bool success{false};
    std::string output;
    std::optional<SessionId> session;
    std::optional<double> costUsd;
};

// The seam between orchestration and execution. In tests this is a fake that
// returns programmed results; in production it is ProcessTaskExecutor.
// Synchronous in M3; concurrent scheduling arrives in M4.
class ITaskExecutor {
public:
    virtual ~ITaskExecutor() = default;
    [[nodiscard]] virtual TaskResult execute(const ExecRequest& request) = 0;
};

} // namespace maestro::orchestrator
