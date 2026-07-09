#pragma once
#include "maestro/process/IProcessBackend.hpp"

namespace maestro::process {

// An IProcessBackend that is driven cooperatively by an owning loop rather than
// its own thread. Both the POSIX (fork/exec/poll) and Qt (QProcess) backends
// implement this so the synchronous ProcessTaskExecutor can pump either one
// without knowing which platform it is on.
class IPumpedBackend : public IProcessBackend {
public:
    // Pump I/O once, blocking up to timeoutMs for activity. Returns true while
    // any process is still running.
    virtual bool processEvents(int timeoutMs) = 0;

    // Block, pumping events, until every started process has exited.
    virtual void runUntilIdle() = 0;
};

} // namespace maestro::process
