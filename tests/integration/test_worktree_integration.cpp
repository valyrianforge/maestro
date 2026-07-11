#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <string>

#include "maestro/worktree/RealGitRunner.hpp"
#include "maestro/worktree/WorktreeManager.hpp"

using namespace maestro::worktree;
namespace fs = std::filesystem;

namespace {

void write(const fs::path& p, const std::string& content) {
    std::ofstream out(p);
    out << content;
}

// A throwaway git repo with one commit. Caller removes the returned root.
fs::path makeRepo(RealGitRunner& git, const std::string& tag) {
    const fs::path root = fs::temp_directory_path() / ("maestro_wt_" + tag);
    fs::remove_all(root);
    fs::create_directories(root);
    const std::string r = root.string();

    REQUIRE(git.run(r, {"init", "-q"}).ok());
    REQUIRE(git.run(r, {"config", "user.email", "t@example.com"}).ok());
    REQUIRE(git.run(r, {"config", "user.name", "Tester"}).ok());
    write(root / "file.txt", "hello\n");
    REQUIRE(git.run(r, {"add", "."}).ok());
    REQUIRE(git.run(r, {"commit", "-q", "-m", "init"}).ok());
    return root;
}

} // namespace

TEST_CASE("real git worktree is created, diffs, and is removed", "[worktree][integration]") {
    RealGitRunner git;
    const fs::path repo = makeRepo(git, "iso");
    const fs::path wtRoot = repo / "_worktrees";

    WorktreeManager mgr(repo.string(), wtRoot.string(), git);

    // Create an isolated worktree for a worker.
    const WorktreeInfo info = mgr.create("worker-0", "HEAD");
    REQUIRE(fs::exists(info.path));
    REQUIRE(fs::exists(fs::path(info.path) / "file.txt")); // base content checked out

    // A clean worktree diffs empty.
    REQUIRE(mgr.diff("worker-0").empty());

    // Modify a tracked file inside the worktree -> diff reflects it.
    write(fs::path(info.path) / "file.txt", "hello\nworld\n");
    const std::string d = mgr.diff("worker-0");
    REQUIRE(d.find("+world") != std::string::npos);

    // Remove tears the worktree down.
    mgr.remove("worker-0");
    REQUIRE_FALSE(fs::exists(info.path));

    fs::remove_all(repo);
}
