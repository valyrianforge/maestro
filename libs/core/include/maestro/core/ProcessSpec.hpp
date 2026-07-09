#pragma once
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace maestro::core {

// A fully-resolved description of how to launch one child process.
// Providers build this; the process layer consumes it. Deliberately free of
// any provider- or Qt-specific concepts so it can live in the Qt-free core.
struct ProcessSpec {
    std::string program;                                  // executable name or path
    std::vector<std::string> args;                        // argv (excluding program)
    std::optional<std::string> workingDirectory;          // cwd; nullopt = inherit
    std::unordered_map<std::string, std::string> env;     // extra/overridden env vars
    bool inheritParentEnv{true};                          // merge process env first
};

// How the child terminated.
enum class ExitReason {
    Exited,   // ran to completion (see exitCode)
    Killed,   // terminated by us via kill()
    Crashed,  // abnormal termination / failed to start
};

struct ProcessExit {
    ExitReason reason{ExitReason::Exited};
    int exitCode{0};

    [[nodiscard]] bool succeeded() const noexcept {
        return reason == ExitReason::Exited && exitCode == 0;
    }
};

// Restart behaviour applied by the ProcessManager on non-successful exit.
struct RestartPolicy {
    int maxRestarts{0};   // 0 = never restart

    [[nodiscard]] static RestartPolicy none() noexcept { return RestartPolicy{0}; }
};

} // namespace maestro::core
