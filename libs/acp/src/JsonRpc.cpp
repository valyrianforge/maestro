#include "maestro/acp/JsonRpc.hpp"

namespace maestro::acp {

JsonRpcMessage parseJsonRpc(std::string_view line) {
    JsonRpcMessage msg;

    // Non-throwing parse: on error nlohmann returns a discarded value.
    const Json j = Json::parse(line, /*cb=*/nullptr, /*allow_exceptions=*/false);
    if (j.is_discarded() || !j.is_object()) {
        msg.kind = MessageKind::Invalid;
        return msg;
    }

    const bool hasMethod = j.contains("method") && j["method"].is_string();
    const bool hasId = j.contains("id") && !j["id"].is_null();
    const bool hasResult = j.contains("result");
    const bool hasError = j.contains("error");

    if (hasMethod) {
        msg.method = j["method"].get<std::string>();
        if (j.contains("params")) {
            msg.params = j["params"];
        }
        if (hasId) {
            msg.kind = MessageKind::Request;
            msg.id = j["id"];
        } else {
            msg.kind = MessageKind::Notification;
        }
        return msg;
    }

    if (hasResult || hasError) {
        msg.kind = MessageKind::Response;
        if (hasId) {
            msg.id = j["id"];
        }
        if (hasError) {
            msg.isError = true;
            msg.error = j["error"];
        } else {
            msg.result = j["result"];
        }
        return msg;
    }

    msg.kind = MessageKind::Invalid;
    return msg;
}

std::string encodeRequest(const Json& id, std::string_view method, const Json& params) {
    Json j;
    j["jsonrpc"] = "2.0";
    j["id"] = id;
    j["method"] = std::string(method);
    if (!params.is_null()) {
        j["params"] = params;
    }
    return j.dump();
}

std::string encodeNotification(std::string_view method, const Json& params) {
    Json j;
    j["jsonrpc"] = "2.0";
    j["method"] = std::string(method);
    if (!params.is_null()) {
        j["params"] = params;
    }
    return j.dump();
}

std::string encodeResponse(const Json& id, const Json& result) {
    Json j;
    j["jsonrpc"] = "2.0";
    j["id"] = id;
    j["result"] = result;
    return j.dump();
}

std::string encodeErrorResponse(const Json& id, int code, std::string_view message) {
    Json j;
    j["jsonrpc"] = "2.0";
    j["id"] = id;
    j["error"] = {{"code", code}, {"message", std::string(message)}};
    return j.dump();
}

} // namespace maestro::acp
