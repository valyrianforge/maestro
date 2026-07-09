#pragma once
#include <functional>
#include <string_view>

#include "maestro/orchestrator/Execution.hpp"
#include "maestro/runtime/ProviderRegistry.hpp"

namespace maestro::runtime {

using orchestrator::ExecRequest;
using orchestrator::ITaskExecutor;
using orchestrator::TaskResult;

// Real executor: resolves the request's provider, builds a ProcessSpec, spawns
// it via ProcessManager + PosixProcessBackend, feeds stdout through the
// provider's frame parser into a ResultCollector, and returns the TaskResult.
// Synchronous (runs the child to completion) to match the M3 orchestrator.
//
// An optional streaming hook lets a UI observe assistant text as it arrives.
class ProcessTaskExecutor final : public ITaskExecutor {
public:
    using StreamHook = std::function<void(const ExecRequest&, std::string_view)>;

    explicit ProcessTaskExecutor(const ProviderRegistry& registry, StreamHook onAssistantText = {});

    [[nodiscard]] TaskResult execute(const ExecRequest& request) override;

private:
    const ProviderRegistry& registry_;
    StreamHook onAssistantText_;
};

} // namespace maestro::runtime
