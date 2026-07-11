#pragma once
#include <string>
#include <vector>

namespace maestro::worktree {

// Result of one git invocation.
struct GitResult {
    int exitCode = 0;
    std::string output; // combined stdout (stderr is merged in by the real runner)

    [[nodiscard]] bool ok() const { return exitCode == 0; }
};

// Seam over running git. The real implementation shells out; the test fake
// records commands and returns canned results, so WorktreeManager is unit-
// testable without touching a real repository.
class IGitRunner {
public:
    virtual ~IGitRunner() = default;

    // Run `git -C <repoDir> <args...>` and return its exit code + output.
    virtual GitResult run(const std::string& repoDir, const std::vector<std::string>& args) = 0;
};

} // namespace maestro::worktree
