#pragma once
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>

#include "maestro/core/ProcessSpec.hpp"
#include "maestro/process/IProcessBackend.hpp"

namespace maestro::process {

using core::RestartPolicy;

// Consumer-facing callbacks for one spawned process. onFinished fires exactly
// once, after any restarts have been exhausted or the process ended for good.
struct ProcessCallbacks {
    std::function<void(std::string_view)> onStdout;
    std::function<void(std::string_view)> onStderr;
    std::function<void(ProcessExit)> onFinished;
};

// Owns process lifecycle policy on top of a transport (IProcessBackend):
// handle allocation, restart-on-failure accounting, and write safety. Holds
// no OS or Qt concepts — it is fully testable against FakeProcessBackend.
//
// Intentional kills (ExitReason::Killed) are never restarted. Non-zero exits
// and crashes are restarted up to RestartPolicy::maxRestarts.
class ProcessManager final : public IProcessBackend::Observer {
public:
    explicit ProcessManager(IProcessBackend& backend);

    // Launch a process. Returns a handle usable with writeStdin()/kill().
    ProcessHandle spawn(ProcessSpec spec, RestartPolicy policy, ProcessCallbacks callbacks);

    // Write to a process's stdin. No-op (safe) if the handle is unknown or the
    // process has already finished.
    void writeStdin(ProcessHandle handle, std::string_view data);

    // Request termination of a running process. No-op if already finished.
    void kill(ProcessHandle handle);

    // Number of processes currently tracked as running (not yet finished).
    [[nodiscard]] std::size_t runningCount() const noexcept;

    // --- IProcessBackend::Observer ---
    void onStdout(ProcessHandle, std::string_view) override;
    void onStderr(ProcessHandle, std::string_view) override;
    void onExit(ProcessHandle, ProcessExit) override;

private:
    struct Entry {
        ProcessSpec spec;
        RestartPolicy policy;
        ProcessCallbacks callbacks;
        int restartsUsed{0};
        bool running{true};
    };

    IProcessBackend& backend_;
    std::unordered_map<ProcessHandle, Entry> entries_;
    ProcessHandle::value_type nextHandle_{1};
};

} // namespace maestro::process
