#pragma once
#include <functional>
#include <memory>
#include <string_view>

#include "maestro/orchestrator/Execution.hpp"
#include "maestro/process/IPumpedBackend.hpp"
#include "maestro/runtime/ProviderRegistry.hpp"

namespace maestro::runtime {

using orchestrator::ExecRequest;
using orchestrator::ITaskExecutor;
using orchestrator::TaskResult;

// Real executor: resolves the request's provider, builds a ProcessSpec, spawns
// it via a fresh backend from the injected factory, feeds stdout through the
// provider's frame parser into a ResultCollector, and returns the TaskResult.
// Synchronous (runs the child to completion) to match the M3 orchestrator.
//
// The backend factory keeps this class platform-agnostic: callers supply a
// PosixProcessBackend factory on macOS/Linux or a QtProcessBackend factory
// anywhere (including Windows). An optional stream hook lets a UI observe
// assistant text as it arrives.
class ProcessTaskExecutor final : public ITaskExecutor {
public:
    using BackendFactory = std::function<std::unique_ptr<process::IPumpedBackend>()>;
    using StreamHook = std::function<void(const ExecRequest&, std::string_view)>;

    ProcessTaskExecutor(const ProviderRegistry& registry, BackendFactory backendFactory,
                        StreamHook onAssistantText = {});

    [[nodiscard]] TaskResult execute(const ExecRequest& request) override;

private:
    const ProviderRegistry& registry_;
    BackendFactory backendFactory_;
    StreamHook onAssistantText_;
};

} // namespace maestro::runtime
