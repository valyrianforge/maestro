#pragma once
#include <string_view>

#include "maestro/core/Ids.hpp"
#include "maestro/core/ProcessSpec.hpp"

namespace maestro::process {

using core::ProcessExit;
using core::ProcessHandle;
using core::ProcessSpec;

// Low-level, OS-facing process abstraction. This is the seam that lets the
// entire process/orchestration stack be tested without launching real
// programs. Real implementation: QtProcessBackend (QProcess). Test double:
// FakeProcessBackend.
//
// A backend is a pure transport: it starts processes, forwards their raw
// output, and reports termination. It has NO restart or policy logic — that
// lives in ProcessManager. The backend never owns handle identity; the caller
// (ProcessManager) allocates handles and passes them in.
class IProcessBackend {
public:
    // Receives raw, unbuffered notifications for every managed process.
    class Observer {
    public:
        virtual ~Observer() = default;
        virtual void onStdout(ProcessHandle, std::string_view chunk) = 0;
        virtual void onStderr(ProcessHandle, std::string_view chunk) = 0;
        virtual void onExit(ProcessHandle, ProcessExit) = 0;
    };

    virtual ~IProcessBackend() = default;

    // Must be called before start(). The backend does not own the observer.
    virtual void setObserver(Observer* observer) = 0;

    // Launch (or relaunch) the process identified by handle. Relaunching an
    // existing handle after it exited is how ProcessManager implements restart.
    virtual void start(ProcessHandle handle, const ProcessSpec& spec) = 0;

    // Write to the child's stdin. Backends may ignore writes to a handle that
    // is not currently running.
    virtual void write(ProcessHandle handle, std::string_view data) = 0;

    // Request termination. The backend must eventually emit onExit with
    // ExitReason::Killed for a running handle.
    virtual void kill(ProcessHandle handle) = 0;
};

} // namespace maestro::process
