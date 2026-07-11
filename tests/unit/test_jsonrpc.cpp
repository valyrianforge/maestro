#include <catch2/catch_test_macros.hpp>

#include "maestro/acp/JsonRpc.hpp"

using namespace maestro::acp;

TEST_CASE("encodeRequest round-trips into a Request message", "[jsonrpc]") {
    const std::string line = encodeRequest(Json(7), "initialize", Json{{"x", 1}});
    const JsonRpcMessage m = parseJsonRpc(line);

    REQUIRE(m.kind == MessageKind::Request);
    REQUIRE(m.method == "initialize");
    REQUIRE(m.id.has_value());
    REQUIRE(m.id->get<int>() == 7);
    REQUIRE(m.params["x"].get<int>() == 1);
}

TEST_CASE("a frame with a method but no id is a Notification", "[jsonrpc]") {
    const std::string line = encodeNotification("session/cancel", Json{{"sessionId", "s1"}});
    const JsonRpcMessage m = parseJsonRpc(line);

    REQUIRE(m.kind == MessageKind::Notification);
    REQUIRE(m.method == "session/cancel");
    REQUIRE(m.params["sessionId"].get<std::string>() == "s1");
    REQUIRE_FALSE(m.id.has_value());
}

TEST_CASE("a result frame is a non-error Response", "[jsonrpc]") {
    const std::string line = encodeResponse(Json(3), Json{{"sessionId", "abc"}});
    const JsonRpcMessage m = parseJsonRpc(line);

    REQUIRE(m.kind == MessageKind::Response);
    REQUIRE_FALSE(m.isError);
    REQUIRE(m.id->get<int>() == 3);
    REQUIRE(m.result["sessionId"].get<std::string>() == "abc");
}

TEST_CASE("an error frame is an error Response", "[jsonrpc]") {
    const std::string line = encodeErrorResponse(Json(4), -32601, "method not found");
    const JsonRpcMessage m = parseJsonRpc(line);

    REQUIRE(m.kind == MessageKind::Response);
    REQUIRE(m.isError);
    REQUIRE(m.error["code"].get<int>() == -32601);
}

TEST_CASE("garbage and non-objects parse to Invalid, not a throw", "[jsonrpc]") {
    REQUIRE(parseJsonRpc("not json at all").kind == MessageKind::Invalid);
    REQUIRE(parseJsonRpc("[1,2,3]").kind == MessageKind::Invalid);
    REQUIRE(parseJsonRpc("{\"jsonrpc\":\"2.0\"}").kind == MessageKind::Invalid);
}
