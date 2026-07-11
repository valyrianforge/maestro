#include <catch2/catch_test_macros.hpp>

#include "maestro/acp/AcpClient.hpp"
#include "maestro/acp/FakeAcpTransport.hpp"
#include "maestro/acp/JsonRpc.hpp"
#include "maestro/acp/SessionController.hpp"

using namespace maestro::acp;

namespace {

// Resolve the most recent session/prompt request as a completed turn.
void completeLastTurn(AcpClient& client, const FakeAcpTransport& transport) {
    for (auto it = transport.sent.rbegin(); it != transport.sent.rend(); ++it) {
        const JsonRpcMessage m = parseJsonRpc(*it);
        if (m.kind == MessageKind::Request && m.method == "session/prompt" && m.id) {
            client.receiveBytes(encodeResponse(*m.id, Json{{"stopReason", "end_turn"}}) + "\n");
            return;
        }
    }
}

// Count session/prompt frames sent so far.
std::size_t promptCount(const FakeAcpTransport& transport) {
    std::size_t n = 0;
    for (const auto& frame : transport.sent) {
        if (parseJsonRpc(frame).method == "session/prompt") ++n;
    }
    return n;
}

} // namespace

TEST_CASE("send while idle starts a turn", "[session]") {
    FakeAcpTransport transport;
    AcpClient client(transport);
    SessionController ctrl(client, "sess-1");

    ctrl.send("do the thing");

    REQUIRE(ctrl.state() == SessionController::State::Busy);
    REQUIRE(promptCount(transport) == 1);
}

TEST_CASE("a completed turn returns to idle and fires onTurnComplete", "[session]") {
    FakeAcpTransport transport;
    AcpClient client(transport);
    SessionController ctrl(client, "sess-1");

    int completed = 0;
    ctrl.onTurnComplete = [&] { ++completed; };

    ctrl.send("go");
    completeLastTurn(client, transport);

    REQUIRE(ctrl.state() == SessionController::State::Idle);
    REQUIRE(completed == 1);
}

TEST_CASE("steering mid-turn queues and auto-delivers on turn end", "[session]") {
    FakeAcpTransport transport;
    AcpClient client(transport);
    SessionController ctrl(client, "sess-1");

    ctrl.send("first task");           // starts turn 1
    ctrl.send("actually also do X");   // mid-turn steer -> queued
    ctrl.send("and Y");                // another steer -> queued

    REQUIRE(ctrl.pending() == 2);
    REQUIRE(promptCount(transport) == 1); // nothing extra sent yet

    completeLastTurn(client, transport); // turn 1 ends -> queued steers flush as turn 2

    REQUIRE(ctrl.pending() == 0);
    REQUIRE(ctrl.state() == SessionController::State::Busy);
    REQUIRE(promptCount(transport) == 2);

    // Turn 2 carried both queued steers as two content blocks.
    const JsonRpcMessage last = parseJsonRpc(transport.sent.back());
    REQUIRE(last.params["prompt"].size() == 2);
    REQUIRE(last.params["prompt"][0]["text"].get<std::string>() == "actually also do X");
    REQUIRE(last.params["prompt"][1]["text"].get<std::string>() == "and Y");
}

TEST_CASE("cancel emits session/cancel and the aborted turn flushes queued redirects", "[session]") {
    FakeAcpTransport transport;
    AcpClient client(transport);
    SessionController ctrl(client, "sess-1");

    ctrl.send("long running task");
    ctrl.send("stop, do this instead"); // queued redirect

    ctrl.cancel();
    // A cancel notification went out.
    bool sawCancel = false;
    for (const auto& frame : transport.sent) {
        if (parseJsonRpc(frame).method == "session/cancel") sawCancel = true;
    }
    REQUIRE(sawCancel);

    // The cancelled turn resolves -> redirect flushes as a new turn.
    completeLastTurn(client, transport);
    REQUIRE(promptCount(transport) == 2);
    REQUIRE(parseJsonRpc(transport.sent.back()).params["prompt"][0]["text"].get<std::string>() ==
            "stop, do this instead");
}
