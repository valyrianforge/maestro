#pragma once
#include <memory>
#include <vector>

#include "maestro/process/IPumpedBackend.hpp"

class QProcess;

namespace maestro::process {

// Cross-platform IPumpedBackend built on QProcess. Unlike PosixProcessBackend
// (POSIX-only), this works on Windows, macOS, and Linux, so it is what the
// packaged desktop app uses. Driven synchronously via QProcess's blocking
// waitFor* calls, so it needs no running Qt event loop and can be pumped from a
// worker thread. Child stdin is closed immediately (headless prompt-via-argv).
class QtProcessBackend final : public IPumpedBackend {
public:
    QtProcessBackend();
    ~QtProcessBackend() override;

    void setObserver(Observer* observer) override;
    void start(ProcessHandle handle, const ProcessSpec& spec) override;
    void write(ProcessHandle handle, std::string_view data) override;
    void kill(ProcessHandle handle) override;
    bool processEvents(int timeoutMs) override;
    void runUntilIdle() override;

private:
    struct Child {
        ProcessHandle handle;
        std::unique_ptr<QProcess> proc;
        bool intentionalKill{false};
    };

    void drain(Child& child);
    void reapByHandle(ProcessHandle handle);

    Observer* observer_{nullptr};
    std::vector<Child> children_;
};

} // namespace maestro::process
