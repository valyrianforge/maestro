#include <catch2/catch_test_macros.hpp>

#include "maestro/acp/AcpClient.hpp"
#include "maestro/acp/FakeAcpTransport.hpp"
#include "maestro/acp/JsonRpc.hpp"

using namespace maestro::acp;

TEST_CASE("initialize sends an 'initialize' request and resolves on response", "[acp]") {
    FakeAcpTransport transport;
    AcpClient client(transport);

    bool resolved = false;
    const int id = client.initialize(Json::object(), [&](const Json& result, const Json* error) {
        resolved = true;
        REQUIRE(error == nullptr);
        REQUIRE(result["protocolVersion"].get<int>() == 1);
    });

    // The client wrote exactly one request frame, with the right method.
    REQUIRE(transport.sent.size() == 1);
    REQUIRE(parseJsonRpc(transport.last()).method == "initialize");

    // Feed the matching response back in; the handler fires exactly once.
    client.receiveBytes(encodeResponse(Json(id), Json{{"protocolVersion", 1}}) + "\n");
    REQUIRE(resolved);
}

TEST_CASE("prompt carries sessionId and content blocks", "[acp]") {
    FakeAcpTransport transport;
    AcpClient client(transport);

    client.prompt("sess-1", Json::array({{{"type", "text"}, {"text", "hello"}}}),
                  [](const Json&, const Json*) {});

    const JsonRpcMessage m = parseJsonRpc(transport.last());
    REQUIRE(m.method == "session/prompt");
    REQUIRE(m.params["sessionId"].get<std::string>() == "sess-1");
    REQUIRE(m.params["prompt"].is_array());
}

TEST_CASE("cancel emits a notification with no id", "[acp]") {
    FakeAcpTransport transport;
    AcpClient client(transport);

    client.cancel("sess-1");

    const JsonRpcMessage m = parseJsonRpc(transport.last());
    REQUIRE(m.kind == MessageKind::Notification);
    REQUIRE(m.method == "session/cancel");
    REQUIRE(m.params["sessionId"].get<std::string>() == "sess-1");
}

TEST_CASE("session/update notifications dispatch as typed SessionUpdates", "[acp]") {
    FakeAcpTransport transport;
    AcpClient client(transport);

    SessionUpdate captured;
    int count = 0;
    client.onSessionUpdate = [&](const SessionUpdate& u) {
        captured = u;
        ++count;
    };

    const Json note = {
        {"jsonrpc", "2.0"},
        {"method", "session/update"},
        {"params",
         {{"sessionId", "sess-1"},
          {"update", {{"sessionUpdate", "agent_message_chunk"}, {"content", {{"text", "hi"}}}}}}}};
    client.receiveBytes(note.dump() + "\n");

    REQUIRE(count == 1);
    REQUIRE(captured.kind == SessionUpdate::Kind::AgentMessageChunk);
    REQUIRE(captured.sessionId == "sess-1");
    REQUIRE(captured.data["content"]["text"].get<std::string>() == "hi");
}

TEST_CASE("permission requests dispatch and can be answered", "[acp]") {
    FakeAcpTransport transport;
    AcpClient client(transport);

    PermissionRequest captured;
    client.onPermissionRequest = [&](const PermissionRequest& r) { captured = r; };

    const Json req = {
        {"jsonrpc", "2.0"},
        {"id", 42},
        {"method", "session/request_permission"},
        {"params",
         {{"sessionId", "sess-1"},
          {"toolCall", {{"kind", "edit"}}},
          {"options", Json::array({{{"optionId", "allow"}, {"name", "Allow"}, {"kind", "allow_once"}},
                                   {{"optionId", "deny"}, {"name", "Deny"}, {"kind", "reject_once"}}})}}}};
    client.receiveBytes(req.dump() + "\n");

    REQUIRE(captured.sessionId == "sess-1");
    REQUIRE(captured.options.size() == 2);
    REQUIRE(captured.options[0].optionId == "allow");

    // Answering echoes the JSON-RPC id and the chosen option back to the agent.
    transport.sent.clear();
    client.respondPermission(captured.requestId, "allow");
    const JsonRpcMessage resp = parseJsonRpc(transport.last());
    REQUIRE(resp.kind == MessageKind::Response);
    REQUIRE(resp.id->get<int>() == 42);
    REQUIRE(resp.result["optionId"].get<std::string>() == "allow");
}

TEST_CASE("frames split across receiveBytes chunks still dispatch once whole", "[acp]") {
    FakeAcpTransport transport;
    AcpClient client(transport);

    int count = 0;
    client.onSessionUpdate = [&](const SessionUpdate&) { ++count; };

    const std::string frame =
        Json{{"jsonrpc", "2.0"},
             {"method", "session/update"},
             {"params", {{"sessionId", "s"}, {"update", {{"sessionUpdate", "plan"}}}}}}
            .dump();

    client.receiveBytes(frame.substr(0, 10)); // partial
    REQUIRE(count == 0);
    client.receiveBytes(frame.substr(10));     // completes the object
    REQUIRE(count == 0);                        // still no newline delimiter
    client.receiveBytes("\n");                  // now it's a full frame
    REQUIRE(count == 1);
}
