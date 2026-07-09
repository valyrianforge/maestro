#pragma once
#include <sys/types.h>

#include <vector>

#include "maestro/process/IProcessBackend.hpp"

namespace maestro::process {

// Real IProcessBackend for POSIX systems (macOS/Linux), built on
// fork/exec/pipe/poll. This is a stepping stone that proves the ProcessManager
// spine against actual OS processes before the cross-platform QtProcessBackend
// exists. Child stdin is redirected from /dev/null (headless, prompt-via-argv
// model), so write() is a safe no-op.
//
// I/O is pumped cooperatively: call processEvents() (or runUntilIdle()) from
// the owning loop. There are no background threads.
class PosixProcessBackend final : public IProcessBackend {
public:
    ~PosixProcessBackend() override;

    void setObserver(Observer* observer) override { observer_ = observer; }
    void start(ProcessHandle handle, const ProcessSpec& spec) override;
    void write(ProcessHandle handle, std::string_view data) override;
    void kill(ProcessHandle handle) override;

    // Pump I/O once, blocking up to timeoutMs for activity. Reads available
    // output, emits callbacks, and reaps finished children. Returns true while
    // any child is still running.
    bool processEvents(int timeoutMs);

    // Block, pumping events, until every started child has exited.
    void runUntilIdle();

    [[nodiscard]] bool anyRunning() const noexcept { return !children_.empty(); }

private:
    struct Child {
        ProcessHandle handle{};
        ::pid_t pid{-1};
        int stdoutFd{-1};
        int stderrFd{-1};
        bool intentionalKill{false};
    };

    void reapByHandle(ProcessHandle handle);

    Observer* observer_{nullptr};
    std::vector<Child> children_;
};

} // namespace maestro::process
