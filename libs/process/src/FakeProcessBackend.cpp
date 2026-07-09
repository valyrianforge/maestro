#include "maestro/process/FakeProcessBackend.hpp"

namespace maestro::process {

void FakeProcessBackend::programStdout(std::string text) {
    programRun(Run{Action{Action::Kind::Stdout, std::move(text), {}}});
}

void FakeProcessBackend::programExit(ProcessExit exit) {
    programRun(Run{Action{Action::Kind::Exit, {}, exit}});
}

void FakeProcessBackend::programRun(Run run) {
    runs_.push_back(std::move(run));
}

void FakeProcessBackend::start(ProcessHandle handle, const ProcessSpec& /*spec*/) {
    ++startCount_;
    running_.insert(handle);

    if (runs_.empty()) {
        return; // no script: process simply "runs" until killed
    }
    const Run run = std::move(runs_.front());
    runs_.pop_front();

    for (const Action& action : run) {
        if (!observer_) {
            break;
        }
        switch (action.kind) {
        case Action::Kind::Stdout:
            observer_->onStdout(handle, action.text);
            break;
        case Action::Kind::Stderr:
            observer_->onStderr(handle, action.text);
            break;
        case Action::Kind::Exit:
            // Mark not-running BEFORE notifying so re-entrant restarts (which
            // call start() again for this handle) observe a clean state.
            running_.erase(handle);
            observer_->onExit(handle, action.exit);
            break;
        }
    }
}

void FakeProcessBackend::write(ProcessHandle handle, std::string_view data) {
    if (!running_.contains(handle)) {
        return; // writes to a non-running process are dropped
    }
    writes_.emplace_back(data);
}

void FakeProcessBackend::kill(ProcessHandle handle) {
    if (!running_.contains(handle)) {
        return;
    }
    running_.erase(handle);
    if (observer_) {
        observer_->onExit(handle, ProcessExit{core::ExitReason::Killed, -1});
    }
}

} // namespace maestro::process
