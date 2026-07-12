// End-to-end proof: an AcpClient drives a REAL agent subprocess over the Agent
// Client Protocol, through the real PosixProcessBackend and a bidirectional
// stdio pipe. The agent is the checked-in deterministic fake-acp-agent (no
// network, no tokens). This validates the whole engine over a real process,
// not just the in-memory fake transport.
#include <catch2/catch_test_macros.hpp>

#include <functional>
#include <string>

#include "maestro/acp/AcpClient.hpp"
#include "maestro/process/PosixProcessBackend.hpp"
#include "maestro/process/ProcessManager.hpp"
#include "maestro/runtime/ProcessAcpTransport.hpp"

using namespace maestro;
using namespace maestro::core;
using namespace maestro::process;

namespace {
ProcessSpec agentSpec() {
    ProcessSpec spec;
    spec.program = FAKE_ACP_AGENT_PATH; // absolute path injected by CMake
    return spec;
}
} // namespace

TEST_CASE("AcpClient drives a real agent subprocess over ACP", "[acp][integration]") {
    PosixProcessBackend backend;
    ProcessManager mgr(backend);
    runtime::ProcessAcpTransport transport(mgr);
    acp::AcpClient client(transport);

    // Route the child's stdout into the client's frame decoder.
    const ProcessHandle handle = mgr.spawn(
        agentSpec(), RestartPolicy::none(),
        ProcessCallbacks{[&](std::string_view s) { client.receiveBytes(s); }, nullptr, nullptr});
    transport.bind(handle);

    // Pump the process loop until `done` or we run out of patience.
    auto pumpUntil = [&](const std::function<bool()>& done) {
        for (int i = 0; i < 100 && !done(); ++i) {
            backend.processEvents(50);
        }
        return done();
    };

    // initialize
    bool initOk = false;
    client.initialize(acp::Json::object(), [&](const acp::Json& result, const acp::Json* error) {
        initOk = (error == nullptr) && result["protocolVersion"].get<int>() == 1;
    });
    REQUIRE(pumpUntil([&] { return initOk; }));

    // session/new
    std::string sessionId;
    client.newSession(acp::Json::object(), [&](const acp::Json& result, const acp::Json*) {
        sessionId = result["sessionId"].get<std::string>();
    });
    REQUIRE(pumpUntil([&] { return !sessionId.empty(); }));
    REQUIRE(sessionId == "s1");

    // session/prompt -> expect a streamed message chunk AND a turn-end response.
    std::string streamed;
    client.onSessionUpdate = [&](const acp::SessionUpdate& u) {
        if (u.kind == acp::SessionUpdate::Kind::AgentMessageChunk) {
            streamed += u.data["content"]["text"].get<std::string>();
        }
    };
    bool turnDone = false;
    client.prompt(sessionId, acp::Json::array({{{"type", "text"}, {"text", "hi"}}}),
                  [&](const acp::Json& result, const acp::Json*) {
                      turnDone = result["stopReason"].get<std::string>() == "end_turn";
                  });
    REQUIRE(pumpUntil([&] { return turnDone && !streamed.empty(); }));
    REQUIRE(streamed == "hello from fake agent");

    mgr.kill(handle); // close stdin -> agent hits EOF and exits
    backend.runUntilIdle();
}
