#pragma once
#include <deque>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "maestro/process/IProcessBackend.hpp"

namespace maestro::process {

// Deterministic, synchronous test double for IProcessBackend.
//
// Behaviour is scripted via "runs". Each call to start() consumes the next
// programmed run (FIFO) and replays its actions immediately and synchronously
// through the observer. This makes restart flows testable without threads or
// timing: e.g. program [exit(1)], [exit(1)], [exit(0)] and a manager with
// maxRestarts>=2 will drive three starts and one final success — all inline.
//
// A run with no Exit action leaves the process "running" until kill() is
// called, which is how streaming/kill scenarios are exercised.
class FakeProcessBackend final : public IProcessBackend {
public:
    struct Action {
        enum class Kind { Stdout, Stderr, Exit };
        Kind kind{Kind::Stdout};
        std::string text;        // for Stdout/Stderr
        ProcessExit exit{};      // for Exit
    };
    using Run = std::vector<Action>;

    // --- Scripting API (call before driving the manager) ---
    void programStdout(std::string text);
    void programExit(ProcessExit exit);
    void programRun(Run run);

    // --- Inspection API (for assertions) ---
    [[nodiscard]] int startCount() const noexcept { return startCount_; }
    [[nodiscard]] const std::vector<std::string>& writes() const noexcept { return writes_; }
    [[nodiscard]] bool isRunning(ProcessHandle h) const { return running_.contains(h); }

    // --- IProcessBackend ---
    void setObserver(Observer* observer) override { observer_ = observer; }
    void start(ProcessHandle handle, const ProcessSpec& spec) override;
    void write(ProcessHandle handle, std::string_view data) override;
    void kill(ProcessHandle handle) override;

private:
    Observer* observer_{nullptr};
    std::deque<Run> runs_;
    std::unordered_set<ProcessHandle> running_;
    std::vector<std::string> writes_;
    int startCount_{0};
};

} // namespace maestro::process
