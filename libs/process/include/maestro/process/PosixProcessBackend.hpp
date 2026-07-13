#pragma once
#include <sys/types.h>

#include <vector>

#include "maestro/process/IPumpedBackend.hpp"

namespace maestro::process {

// Real IProcessBackend for POSIX systems (macOS/Linux), built on
// fork/exec/pipe/poll. This is a stepping stone that proves the ProcessManager
// spine against actual OS processes before the cross-platform QtProcessBackend
// exists. Child stdin is a real pipe, so write() delivers bytes to the child —
// this is what lets an interactive, bidirectional ACP agent be driven over
// stdio. Closing our end (on kill/destroy) signals EOF to the child.
//
// I/O is pumped cooperatively: call processEvents() (or runUntilIdle()) from
// the owning loop. There are no background threads.
class PosixProcessBackend final : public IPumpedBackend {
public:
    ~PosixProcessBackend() override;

    void setObserver(Observer* observer) override { observer_ = observer; }
    void start(ProcessHandle handle, const ProcessSpec& spec) override;
    void write(ProcessHandle handle, std::string_view data) override;
    void kill(ProcessHandle handle) override;

    // Pump I/O once, blocking up to timeoutMs for activity. Reads available
    // output, emits callbacks, and reaps finished children. Returns true while
    // any child is still running.
    bool processEvents(int timeoutMs) override;

    // Block, pumping events, until every started child has exited.
    void runUntilIdle() override;

    [[nodiscard]] bool anyRunning() const noexcept { return !children_.empty(); }

private:
    struct Child {
        ProcessHandle handle{};
        ::pid_t pid{-1};
        int stdinFd{-1}; // parent's write end of the child's stdin pipe
        int stdoutFd{-1};
        int stderrFd{-1};
        bool intentionalKill{false};
    };

    void reapByHandle(ProcessHandle handle);

    Observer* observer_{nullptr};
    std::vector<Child> children_;
};

} // namespace maestro::process
