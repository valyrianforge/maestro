#pragma once
#include <string>
#include <vector>

#include "maestro/worktree/IGitRunner.hpp"

namespace maestro::worktree {

// Test double: records every git invocation and returns a canned result
// (success/empty by default). Lets tests assert the exact command sequence
// WorktreeManager issues without touching a real repository.
class FakeGitRunner : public IGitRunner {
public:
    struct Call {
        std::string repoDir;
        std::vector<std::string> args;
    };

    GitResult run(const std::string& repoDir, const std::vector<std::string>& args) override {
        calls.push_back({repoDir, args});
        if (nextExitCode != 0) {
            const int code = nextExitCode;
            nextExitCode = 0; // one-shot failure
            return {code, nextOutput};
        }
        return {0, nextOutput};
    }

    // The most recent call's joined args, for concise assertions.
    [[nodiscard]] std::string lastArgs() const {
        std::string joined;
        for (const auto& a : calls.back().args) {
            if (!joined.empty()) joined += ' ';
            joined += a;
        }
        return joined;
    }

    std::vector<Call> calls;
    std::string nextOutput; // result output to return
    int nextExitCode = 0;   // set non-zero to make the NEXT run() fail once
};

} // namespace maestro::worktree
