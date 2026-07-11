#include "maestro/worktree/WorktreeManager.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace maestro::worktree {

WorktreeManager::WorktreeManager(std::string repoDir, std::string worktreesRoot, IGitRunner& git)
    : repoDir_(std::move(repoDir)), worktreesRoot_(std::move(worktreesRoot)), git_(git) {}

std::string WorktreeManager::branchFor(const std::string& workerId) {
    return "maestro/" + workerId;
}

WorktreeInfo WorktreeManager::create(const std::string& workerId, const std::string& baseRef) {
    if (const auto it = active_.find(workerId); it != active_.end()) {
        return it->second;
    }

    WorktreeInfo info;
    info.workerId = workerId;
    info.branch = branchFor(workerId);
    info.path = worktreesRoot_ + "/" + workerId;

    // `git worktree add -b <branch> <path> <baseRef>` creates the branch and a
    // linked working tree in one step.
    const GitResult res = git_.run(
        repoDir_, {"worktree", "add", "-b", info.branch, info.path, baseRef});
    if (!res.ok()) {
        throw std::runtime_error("git worktree add failed for '" + workerId + "': " + res.output);
    }

    active_.emplace(workerId, info);
    order_.push_back(workerId);
    return info;
}

std::string WorktreeManager::diff(const std::string& workerId, const std::string& baseRef) {
    const auto it = active_.find(workerId);
    if (it == active_.end()) {
        throw std::runtime_error("no worktree for worker '" + workerId + "'");
    }
    // Diff the worktree's working state against the base ref.
    const GitResult res = git_.run(it->second.path, {"diff", baseRef});
    if (!res.ok()) {
        throw std::runtime_error("git diff failed for '" + workerId + "': " + res.output);
    }
    return res.output;
}

void WorktreeManager::remove(const std::string& workerId) {
    const auto it = active_.find(workerId);
    if (it == active_.end()) {
        return;
    }
    git_.run(repoDir_, {"worktree", "remove", "--force", it->second.path});
    git_.run(repoDir_, {"branch", "-D", it->second.branch});
    active_.erase(it);
    order_.erase(std::remove(order_.begin(), order_.end(), workerId), order_.end());
}

std::optional<WorktreeInfo> WorktreeManager::get(const std::string& workerId) const {
    const auto it = active_.find(workerId);
    if (it == active_.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::vector<WorktreeInfo> WorktreeManager::worktrees() const {
    std::vector<WorktreeInfo> out;
    out.reserve(order_.size());
    for (const auto& id : order_) {
        out.push_back(active_.at(id));
    }
    return out;
}

} // namespace maestro::worktree
