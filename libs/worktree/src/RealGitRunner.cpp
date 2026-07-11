#include "maestro/worktree/RealGitRunner.hpp"

#include <array>
#include <cstdio>
#include <string>

namespace maestro::worktree {
namespace {

// Wrap a token in single quotes, escaping any embedded single quotes, so it
// survives the shell as one literal argument.
std::string shellQuote(const std::string& s) {
    std::string out = "'";
    for (const char c : s) {
        if (c == '\'') {
            out += "'\\''";
        } else {
            out += c;
        }
    }
    out += "'";
    return out;
}

} // namespace

GitResult RealGitRunner::run(const std::string& repoDir, const std::vector<std::string>& args) {
    std::string cmd = "git -C " + shellQuote(repoDir);
    for (const auto& a : args) {
        cmd += ' ';
        cmd += shellQuote(a);
    }
    cmd += " 2>&1"; // merge stderr into stdout

    std::FILE* pipe = ::popen(cmd.c_str(), "r");
    if (pipe == nullptr) {
        return {-1, "failed to start git"};
    }

    std::string output;
    std::array<char, 4096> buf{};
    while (std::fgets(buf.data(), static_cast<int>(buf.size()), pipe) != nullptr) {
        output += buf.data();
    }

    const int status = ::pclose(pipe);
    // popen/pclose returns a wait(2)-style status; extract the exit code.
    const int exitCode = (status == -1) ? -1 : (status >> 8) & 0xFF;
    return {exitCode, output};
}

} // namespace maestro::worktree
