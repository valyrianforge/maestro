#pragma once
#include "maestro/worktree/IGitRunner.hpp"

namespace maestro::worktree {

// Runs git as a real child process (POSIX popen), merging stderr into stdout.
// Used in production and in the integration test against a temp repository.
class RealGitRunner : public IGitRunner {
public:
    GitResult run(const std::string& repoDir, const std::vector<std::string>& args) override;
};

} // namespace maestro::worktree
