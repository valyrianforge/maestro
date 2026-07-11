#pragma once
#include <optional>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

namespace maestro::acp {

using Json = nlohmann::json;

// The four shapes a JSON-RPC 2.0 frame can take on the wire. `Invalid` is used
// for anything that does not parse or does not match a known shape, so callers
// never have to catch exceptions.
enum class MessageKind { Request, Response, Notification, Invalid };

// A decoded JSON-RPC frame. Only the fields relevant to `kind` are populated.
//   Request      -> id, method, params
//   Notification -> method, params
//   Response      -> id, and exactly one of {result} or {isError + error}
struct JsonRpcMessage {
    MessageKind kind = MessageKind::Invalid;
    std::optional<Json> id; // JSON-RPC ids may be number or string; kept as-is.
    std::string method;
    Json params;
    Json result;
    bool isError = false;
    Json error;
};

// Parse one JSON-RPC frame (a single line of JSON). Never throws: malformed
// input yields a message with kind == Invalid.
JsonRpcMessage parseJsonRpc(std::string_view line);

// Encoders. Each returns a single compact JSON line (no trailing newline); the
// transport is responsible for newline framing.
std::string encodeRequest(const Json& id, std::string_view method, const Json& params);
std::string encodeNotification(std::string_view method, const Json& params);
std::string encodeResponse(const Json& id, const Json& result);
std::string encodeErrorResponse(const Json& id, int code, std::string_view message);

} // namespace maestro::acp
