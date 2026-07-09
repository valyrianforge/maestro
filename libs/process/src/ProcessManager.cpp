#include "maestro/process/ProcessManager.hpp"

#include <algorithm>

namespace maestro::process {

ProcessManager::ProcessManager(IProcessBackend& backend) : backend_(backend) {
    backend_.setObserver(this);
}

ProcessHandle ProcessManager::spawn(ProcessSpec spec, RestartPolicy policy,
                                    ProcessCallbacks callbacks) {
    const ProcessHandle handle{nextHandle_++};
    entries_.emplace(handle, Entry{std::move(spec), policy, std::move(callbacks), 0, true});
    // Copy the spec for the backend from the stored entry so restarts reuse it.
    backend_.start(handle, entries_.at(handle).spec);
    return handle;
}

void ProcessManager::writeStdin(ProcessHandle handle, std::string_view data) {
    const auto it = entries_.find(handle);
    if (it == entries_.end() || !it->second.running) {
        return; // unknown or finished: safe no-op
    }
    backend_.write(handle, data);
}

void ProcessManager::kill(ProcessHandle handle) {
    const auto it = entries_.find(handle);
    if (it == entries_.end() || !it->second.running) {
        return;
    }
    backend_.kill(handle);
}

std::size_t ProcessManager::runningCount() const noexcept {
    return static_cast<std::size_t>(
        std::count_if(entries_.begin(), entries_.end(),
                      [](const auto& kv) { return kv.second.running; }));
}

void ProcessManager::onStdout(ProcessHandle handle, std::string_view chunk) {
    const auto it = entries_.find(handle);
    if (it != entries_.end() && it->second.callbacks.onStdout) {
        it->second.callbacks.onStdout(chunk);
    }
}

void ProcessManager::onStderr(ProcessHandle handle, std::string_view chunk) {
    const auto it = entries_.find(handle);
    if (it != entries_.end() && it->second.callbacks.onStderr) {
        it->second.callbacks.onStderr(chunk);
    }
}

void ProcessManager::onExit(ProcessHandle handle, ProcessExit exit) {
    const auto it = entries_.find(handle);
    if (it == entries_.end()) {
        return;
    }
    Entry& entry = it->second;

    const bool intentionalKill = exit.reason == core::ExitReason::Killed;
    const bool canRestart =
        !exit.succeeded() && !intentionalKill && entry.restartsUsed < entry.policy.maxRestarts;

    if (canRestart) {
        ++entry.restartsUsed;
        backend_.start(handle, entry.spec); // may re-enter onExit synchronously
        return;
    }

    entry.running = false;
    // Copy the callback before invoking: the callback may outlive/erase state.
    auto onFinished = entry.callbacks.onFinished;
    if (onFinished) {
        onFinished(exit);
    }
}

} // namespace maestro::process
