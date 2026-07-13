// Integration tests: drive the REAL PosixProcessBackend through ProcessManager
// against harmless system commands (no network, no Claude tokens). Proves the
// spawn/stream/exit path works against actual OS processes.
#include <catch2/catch_test_macros.hpp>

#include <string>

#include "maestro/process/PosixProcessBackend.hpp"
#include "maestro/process/ProcessManager.hpp"

using namespace maestro::core;
using namespace maestro::process;

namespace {
ProcessSpec exec(std::string program, std::vector<std::string> args) {
    ProcessSpec spec;
    spec.program = std::move(program);
    spec.args = std::move(args);
    return spec;
}
} // namespace

TEST_CASE("real process: stdout is captured and clean exit is success", "[integration]") {
    PosixProcessBackend backend;
    ProcessManager mgr(backend);

    std::string out;
    ProcessExit result{ExitReason::Crashed, -1};
    mgr.spawn(exec("echo", {"maestro-live"}), RestartPolicy::none(),
              ProcessCallbacks{[&](std::string_view s) { out += s; }, nullptr,
                               [&](ProcessExit e) { result = e; }});
    backend.runUntilIdle();

    REQUIRE(out == "maestro-live\n");
    REQUIRE(result.succeeded());
}

TEST_CASE("real process: non-zero exit code is reported", "[integration]") {
    PosixProcessBackend backend;
    ProcessManager mgr(backend);

    ProcessExit result{ExitReason::Exited, 0};
    mgr.spawn(exec("sh", {"-c", "exit 3"}), RestartPolicy::none(),
              ProcessCallbacks{nullptr, nullptr, [&](ProcessExit e) { result = e; }});
    backend.runUntilIdle();

    REQUIRE_FALSE(result.succeeded());
    REQUIRE(result.reason == ExitReason::Exited);
    REQUIRE(result.exitCode == 3);
}

TEST_CASE("real process: stderr is routed separately from stdout", "[integration]") {
    PosixProcessBackend backend;
    ProcessManager mgr(backend);

    std::string out, err;
    mgr.spawn(exec("sh", {"-c", "echo good; echo bad 1>&2"}), RestartPolicy::none(),
              ProcessCallbacks{[&](std::string_view s) { out += s; },
                               [&](std::string_view s) { err += s; }, nullptr});
    backend.runUntilIdle();

    REQUIRE(out == "good\n");
    REQUIRE(err == "bad\n");
}

TEST_CASE("real process: bytes written to stdin reach the child and echo back", "[integration]") {
    PosixProcessBackend backend;
    ProcessManager mgr(backend);

    std::string out;
    const ProcessHandle h = mgr.spawn(
        exec("cat", {}), RestartPolicy::none(),
        ProcessCallbacks{[&](std::string_view s) { out += s; }, nullptr, nullptr});

    // cat echoes stdin to stdout: proves the child's stdin pipe is writable.
    mgr.writeStdin(h, "ping-over-stdin\n");
    for (int i = 0; i < 50 && out.find('\n') == std::string::npos; ++i) {
        backend.processEvents(50);
    }
    REQUIRE(out == "ping-over-stdin\n");

    mgr.kill(h); // cat runs until EOF/signal; end the session
    backend.runUntilIdle();
}

TEST_CASE("real process: a missing program is reported as failure, not a crash of us",
          "[integration]") {
    PosixProcessBackend backend;
    ProcessManager mgr(backend);

    ProcessExit result{ExitReason::Exited, 0};
    mgr.spawn(exec("maestro_no_such_program_xyz", {}), RestartPolicy::none(),
              ProcessCallbacks{nullptr, nullptr, [&](ProcessExit e) { result = e; }});
    backend.runUntilIdle();

    REQUIRE_FALSE(result.succeeded()); // execvp failed -> child _exit(127)
}
