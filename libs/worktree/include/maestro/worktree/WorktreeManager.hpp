#pragma once
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "maestro/worktree/IGitRunner.hpp"

namespace maestro::worktree {

// One worker's isolated checkout.
struct WorktreeInfo {
    std::string workerId;
    std::string path;   // absolute path of the worktree on disk
    std::string branch; // branch checked out there (e.g. "maestro/worker-0")
};

// Gives each worker (electron) its own git worktree on its own branch, so
// parallel workers edit the same repository without colliding. The nucleus /
// human reviews each worker's diff before it is integrated.
//
// All git access goes through IGitRunner, so the command sequence is unit-
// testable against a fake, and a real run is covered by an integration test.
class WorktreeManager {
public:
    // repoDir: the project git repo. worktreesRoot: directory under which each
    // worker's worktree is created (one subdirectory per worker).
    WorktreeManager(std::string repoDir, std::string worktreesRoot, IGitRunner& git);

    // Create a worktree for workerId on a fresh branch off baseRef. Throws
    // std::runtime_error if git fails. Idempotent per workerId (returns the
    // existing one if already created).
    WorktreeInfo create(const std::string& workerId, const std::string& baseRef = "HEAD");

    // Unified diff of the worker's worktree against baseRef.
    std::string diff(const std::string& workerId, const std::string& baseRef = "HEAD");

    // Remove the worktree (and prune its branch).
    void remove(const std::string& workerId);

    [[nodiscard]] std::optional<WorktreeInfo> get(const std::string& workerId) const;
    [[nodiscard]] std::vector<WorktreeInfo> worktrees() const;
    [[nodiscard]] std::size_t size() const { return active_.size(); }

    // Branch name this manager assigns to a worker.
    [[nodiscard]] static std::string branchFor(const std::string& workerId);

private:
    std::string repoDir_;
    std::string worktreesRoot_;
    IGitRunner& git_;
    std::unordered_map<std::string, WorktreeInfo> active_;
    std::vector<std::string> order_;
};

} // namespace maestro::worktree
