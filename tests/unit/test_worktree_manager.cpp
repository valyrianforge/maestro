#include <catch2/catch_test_macros.hpp>

#include <stdexcept>

#include "maestro/worktree/FakeGitRunner.hpp"
#include "maestro/worktree/WorktreeManager.hpp"

using namespace maestro::worktree;

TEST_CASE("create issues 'git worktree add -b' and records the worktree", "[worktree]") {
    FakeGitRunner git;
    WorktreeManager mgr("/repo", "/tmp/wt", git);

    const WorktreeInfo info = mgr.create("worker-0", "main");

    REQUIRE(info.branch == "maestro/worker-0");
    REQUIRE(info.path == "/tmp/wt/worker-0");
    REQUIRE(mgr.size() == 1);

    REQUIRE(git.calls.size() == 1);
    REQUIRE(git.calls[0].repoDir == "/repo");
    REQUIRE(git.lastArgs() == "worktree add -b maestro/worker-0 /tmp/wt/worker-0 main");
}

TEST_CASE("create is idempotent per worker id", "[worktree]") {
    FakeGitRunner git;
    WorktreeManager mgr("/repo", "/tmp/wt", git);

    mgr.create("worker-0");
    mgr.create("worker-0"); // second call must not issue another git command

    REQUIRE(mgr.size() == 1);
    REQUIRE(git.calls.size() == 1);
}

TEST_CASE("a git failure on create throws", "[worktree]") {
    FakeGitRunner git;
    git.nextExitCode = 128;
    git.nextOutput = "fatal: branch already exists";
    WorktreeManager mgr("/repo", "/tmp/wt", git);

    REQUIRE_THROWS_AS(mgr.create("worker-0"), std::runtime_error);
    REQUIRE(mgr.size() == 0); // not recorded on failure
}

TEST_CASE("diff runs 'git diff' inside the worktree path", "[worktree]") {
    FakeGitRunner git;
    WorktreeManager mgr("/repo", "/tmp/wt", git);
    mgr.create("worker-0");

    git.nextOutput = "diff --git a/x b/x\n+change\n";
    const std::string d = mgr.diff("worker-0", "HEAD");

    REQUIRE(d.find("+change") != std::string::npos);
    REQUIRE(git.calls.back().repoDir == "/tmp/wt/worker-0");
    REQUIRE(git.lastArgs() == "diff HEAD");
}

TEST_CASE("remove prunes the worktree and its branch", "[worktree]") {
    FakeGitRunner git;
    WorktreeManager mgr("/repo", "/tmp/wt", git);
    mgr.create("worker-0");
    git.calls.clear();

    mgr.remove("worker-0");

    REQUIRE(mgr.size() == 0);
    REQUIRE(git.calls.size() == 2);
    REQUIRE(git.calls[0].args[0] == "worktree"); // worktree remove --force ...
    REQUIRE(git.calls[1].args[0] == "branch");    // branch -D maestro/worker-0
}
