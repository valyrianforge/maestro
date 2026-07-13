#include "maestro/process/PosixProcessBackend.hpp"

#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <string>
#include <utility>

namespace maestro::process {
namespace {

void setNonBlocking(int fd) {
    const int flags = ::fcntl(fd, F_GETFL, 0);
    if (flags >= 0) {
        ::fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    }
}

// Drain a readable fd. Returns false once EOF is seen (caller should close it).
bool drain(int fd, ProcessHandle handle, IProcessBackend::Observer* obs, bool isStdout) {
    char buf[4096];
    for (;;) {
        const ssize_t n = ::read(fd, buf, sizeof(buf));
        if (n > 0) {
            if (obs) {
                const std::string_view chunk{buf, static_cast<std::size_t>(n)};
                if (isStdout) {
                    obs->onStdout(handle, chunk);
                } else {
                    obs->onStderr(handle, chunk);
                }
            }
            continue;
        }
        if (n == 0) {
            return false; // EOF
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return true; // no more data for now, still open
        }
        if (errno == EINTR) {
            continue;
        }
        return false; // hard error: treat as closed
    }
}

} // namespace

PosixProcessBackend::~PosixProcessBackend() {
    for (Child& c : children_) {
        if (c.pid > 0) {
            ::kill(c.pid, SIGKILL);
            int status = 0;
            ::waitpid(c.pid, &status, 0);
        }
        if (c.stdinFd >= 0) ::close(c.stdinFd);
        if (c.stdoutFd >= 0) ::close(c.stdoutFd);
        if (c.stderrFd >= 0) ::close(c.stderrFd);
    }
}

void PosixProcessBackend::start(ProcessHandle handle, const ProcessSpec& spec) {
    int inPipe[2];
    int outPipe[2];
    int errPipe[2];
    if (::pipe(inPipe) != 0 || ::pipe(outPipe) != 0 || ::pipe(errPipe) != 0) {
        if (observer_) observer_->onExit(handle, ProcessExit{core::ExitReason::Crashed, -1});
        return;
    }

    const ::pid_t pid = ::fork();
    if (pid < 0) {
        ::close(inPipe[0]); ::close(inPipe[1]);
        ::close(outPipe[0]); ::close(outPipe[1]);
        ::close(errPipe[0]); ::close(errPipe[1]);
        if (observer_) observer_->onExit(handle, ProcessExit{core::ExitReason::Crashed, -1});
        return;
    }

    if (pid == 0) {
        // --- child ---
        ::dup2(inPipe[0], STDIN_FILENO);
        ::dup2(outPipe[1], STDOUT_FILENO);
        ::dup2(errPipe[1], STDERR_FILENO);
        ::close(inPipe[0]); ::close(inPipe[1]);
        ::close(outPipe[0]); ::close(outPipe[1]);
        ::close(errPipe[0]); ::close(errPipe[1]);

        if (spec.workingDirectory) {
            if (::chdir(spec.workingDirectory->c_str()) != 0) {
                ::_exit(126);
            }
        }
        for (const auto& [key, value] : spec.env) {
            ::setenv(key.c_str(), value.c_str(), 1);
        }

        std::vector<std::string> parts;
        parts.reserve(spec.args.size() + 1);
        parts.push_back(spec.program);
        for (const auto& a : spec.args) {
            parts.push_back(a);
        }
        std::vector<char*> argv;
        argv.reserve(parts.size() + 1);
        for (auto& s : parts) {
            argv.push_back(s.data());
        }
        argv.push_back(nullptr);

        ::execvp(spec.program.c_str(), argv.data());
        ::_exit(127); // exec failed (e.g. program not found)
    }

    // --- parent ---
    ::close(inPipe[0]);  // parent keeps the write end
    ::close(outPipe[1]);
    ::close(errPipe[1]);
    setNonBlocking(outPipe[0]);
    setNonBlocking(errPipe[0]);
    children_.push_back(Child{handle, pid, inPipe[1], outPipe[0], errPipe[0], false});
}

void PosixProcessBackend::write(ProcessHandle handle, std::string_view data) {
    const auto it = std::find_if(children_.begin(), children_.end(),
                                 [&](const Child& c) { return c.handle == handle; });
    if (it == children_.end() || it->stdinFd < 0) {
        return; // unknown handle or stdin already closed: safe no-op
    }

    // Write all bytes, tolerating partial writes and EINTR. A broken pipe
    // (child gone) is swallowed — it surfaces as an exit via the output streams.
    std::size_t offset = 0;
    while (offset < data.size()) {
        const ssize_t n = ::write(it->stdinFd, data.data() + offset, data.size() - offset);
        if (n > 0) {
            offset += static_cast<std::size_t>(n);
            continue;
        }
        if (n < 0 && errno == EINTR) {
            continue;
        }
        break; // EAGAIN on a full buffer, or EPIPE: stop
    }
}

void PosixProcessBackend::kill(ProcessHandle handle) {
    const auto it = std::find_if(children_.begin(), children_.end(),
                                 [&](const Child& c) { return c.handle == handle; });
    if (it == children_.end()) {
        return;
    }
    it->intentionalKill = true;
    if (it->pid > 0) {
        ::kill(it->pid, SIGTERM);
    }
}

bool PosixProcessBackend::processEvents(int timeoutMs) {
    if (children_.empty()) {
        return false;
    }

    std::vector<::pollfd> pfds;
    std::vector<std::pair<ProcessHandle, bool>> owners; // (handle, isStdout)
    for (const Child& c : children_) {
        if (c.stdoutFd >= 0) {
            pfds.push_back(::pollfd{c.stdoutFd, static_cast<short>(POLLIN), 0});
            owners.emplace_back(c.handle, true);
        }
        if (c.stderrFd >= 0) {
            pfds.push_back(::pollfd{c.stderrFd, static_cast<short>(POLLIN), 0});
            owners.emplace_back(c.handle, false);
        }
    }
    if (pfds.empty()) {
        return anyRunning();
    }

    const int rc = ::poll(pfds.data(), static_cast<nfds_t>(pfds.size()), timeoutMs);
    if (rc < 0) {
        return anyRunning(); // EINTR or transient: try again next tick
    }

    for (std::size_t i = 0; i < pfds.size(); ++i) {
        if ((pfds[i].revents & (POLLIN | POLLHUP | POLLERR)) == 0) {
            continue;
        }
        const auto [handle, isStdout] = owners[i];
        const auto it = std::find_if(children_.begin(), children_.end(),
                                     [&](const Child& c) { return c.handle == handle; });
        if (it == children_.end()) {
            continue;
        }
        const bool stillOpen = drain(pfds[i].fd, handle, observer_, isStdout);
        if (!stillOpen) {
            ::close(pfds[i].fd);
            if (isStdout) {
                it->stdoutFd = -1;
            } else {
                it->stderrFd = -1;
            }
        }
    }

    // Reap children whose streams are both closed. Collect handles first so the
    // onExit callback (which may re-start the same handle) can't invalidate us.
    std::vector<ProcessHandle> toReap;
    for (const Child& c : children_) {
        if (c.stdoutFd < 0 && c.stderrFd < 0) {
            toReap.push_back(c.handle);
        }
    }
    for (const ProcessHandle h : toReap) {
        reapByHandle(h);
    }

    return anyRunning();
}

void PosixProcessBackend::reapByHandle(ProcessHandle handle) {
    const auto it = std::find_if(children_.begin(), children_.end(),
                                 [&](const Child& c) { return c.handle == handle; });
    if (it == children_.end()) {
        return;
    }
    const ::pid_t pid = it->pid;
    const bool killed = it->intentionalKill;

    if (it->stdinFd >= 0) {
        ::close(it->stdinFd); // signals EOF to the child and frees the fd
        it->stdinFd = -1;
    }

    int status = 0;
    ::waitpid(pid, &status, 0);

    ProcessExit exit{core::ExitReason::Crashed, -1};
    if (killed) {
        exit = ProcessExit{core::ExitReason::Killed, -1};
    } else if (WIFEXITED(status)) {
        exit = ProcessExit{core::ExitReason::Exited, WEXITSTATUS(status)};
    } else if (WIFSIGNALED(status)) {
        exit = ProcessExit{core::ExitReason::Crashed, -1};
    }

    children_.erase(it); // erase before notifying: a restart may push a new child
    if (observer_) {
        observer_->onExit(handle, exit);
    }
}

void PosixProcessBackend::runUntilIdle() {
    while (anyRunning()) {
        processEvents(200);
    }
}

} // namespace maestro::process
