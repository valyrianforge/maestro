#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "maestro/process/FakeProcessBackend.hpp"
#include "maestro/process/ProcessManager.hpp"

using namespace maestro::core;
using namespace maestro::process;

namespace {
ProcessSpec sampleSpec() {
    ProcessSpec spec;
    spec.program = "claude";
    spec.args = {"-p", "do work"};
    return spec;
}
} // namespace

TEST_CASE("stdout from the backend is routed to the consumer callback", "[process]") {
    FakeProcessBackend backend;
    backend.programStdout("hello world");

    ProcessManager mgr(backend);
    std::string captured;
    mgr.spawn(sampleSpec(), RestartPolicy::none(),
              ProcessCallbacks{
                  [&](std::string_view s) { captured += s; }, nullptr, nullptr});

    REQUIRE(captured == "hello world");
    REQUIRE(backend.startCount() == 1);
    REQUIRE(mgr.runningCount() == 1); // no exit scripted -> still running
}

TEST_CASE("a clean exit reports success and does not restart", "[process]") {
    FakeProcessBackend backend;
    backend.programExit(ProcessExit{ExitReason::Exited, 0});

    ProcessManager mgr(backend);
    ProcessExit finished{ExitReason::Crashed, -99};
    int finishCount = 0;
    mgr.spawn(sampleSpec(), RestartPolicy{2},
              ProcessCallbacks{nullptr, nullptr, [&](ProcessExit e) {
                                   finished = e;
                                   ++finishCount;
                               }});

    REQUIRE(finishCount == 1);
    REQUIRE(finished.succeeded());
    REQUIRE(backend.startCount() == 1); // never restarted
    REQUIRE(mgr.runningCount() == 0);
}

TEST_CASE("a failing process is restarted up to the policy limit then recovers", "[process]") {
    FakeProcessBackend backend;
    backend.programExit(ProcessExit{ExitReason::Exited, 1}); // run 1: fail
    backend.programExit(ProcessExit{ExitReason::Exited, 1}); // run 2: fail
    backend.programExit(ProcessExit{ExitReason::Exited, 0}); // run 3: success

    ProcessManager mgr(backend);
    ProcessExit finished{ExitReason::Crashed, -99};
    int finishCount = 0;
    mgr.spawn(sampleSpec(), RestartPolicy{2},
              ProcessCallbacks{nullptr, nullptr, [&](ProcessExit e) {
                                   finished = e;
                                   ++finishCount;
                               }});

    REQUIRE(backend.startCount() == 3);   // initial + 2 restarts
    REQUIRE(finishCount == 1);            // onFinished fires exactly once
    REQUIRE(finished.succeeded());
}

TEST_CASE("restarts are exhausted and the final failure is reported", "[process]") {
    FakeProcessBackend backend;
    for (int i = 0; i < 3; ++i) {
        backend.programExit(ProcessExit{ExitReason::Exited, 7});
    }

    ProcessManager mgr(backend);
    ProcessExit finished{ExitReason::Exited, 0};
    mgr.spawn(sampleSpec(), RestartPolicy{2},
              ProcessCallbacks{nullptr, nullptr, [&](ProcessExit e) { finished = e; }});

    REQUIRE(backend.startCount() == 3); // initial + 2 restarts, then give up
    REQUIRE_FALSE(finished.succeeded());
    REQUIRE(finished.exitCode == 7);
}

TEST_CASE("an intentional kill is never restarted", "[process]") {
    FakeProcessBackend backend; // no exit scripted -> stays running

    ProcessManager mgr(backend);
    ProcessExit finished{ExitReason::Exited, 0};
    int finishCount = 0;
    const ProcessHandle h =
        mgr.spawn(sampleSpec(), RestartPolicy{5},
                  ProcessCallbacks{nullptr, nullptr, [&](ProcessExit e) {
                                       finished = e;
                                       ++finishCount;
                                   }});

    mgr.kill(h);

    REQUIRE(finishCount == 1);
    REQUIRE(finished.reason == ExitReason::Killed);
    REQUIRE(backend.startCount() == 1); // never restarted despite policy of 5
    REQUIRE(mgr.runningCount() == 0);
}

TEST_CASE("writing to stdin after finish is a safe no-op", "[process]") {
    FakeProcessBackend backend;

    ProcessManager mgr(backend);
    const ProcessHandle h = mgr.spawn(sampleSpec(), RestartPolicy::none(),
                                      ProcessCallbacks{nullptr, nullptr, nullptr});

    mgr.writeStdin(h, "before-kill");
    REQUIRE(backend.writes().size() == 1);

    mgr.kill(h);
    mgr.writeStdin(h, "after-kill"); // must not crash, must not reach backend
    REQUIRE(backend.writes().size() == 1);

    // Writing to a never-issued handle is also safe.
    mgr.writeStdin(ProcessHandle{9999}, "ghost");
    REQUIRE(backend.writes().size() == 1);
}
