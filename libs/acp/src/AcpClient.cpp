#include "maestro/acp/AcpClient.hpp"

namespace maestro::acp {
namespace {

SessionUpdate::Kind updateKindFromString(std::string_view s) {
    if (s == "agent_message_chunk") return SessionUpdate::Kind::AgentMessageChunk;
    if (s == "agent_thought_chunk") return SessionUpdate::Kind::AgentThoughtChunk;
    if (s == "plan") return SessionUpdate::Kind::Plan;
    if (s == "tool_call") return SessionUpdate::Kind::ToolCall;
    if (s == "tool_call_update") return SessionUpdate::Kind::ToolCallUpdate;
    if (s == "usage_update") return SessionUpdate::Kind::UsageUpdate;
    return SessionUpdate::Kind::Unknown;
}

} // namespace

AcpClient::AcpClient(IAcpTransport& transport) : transport_(transport) {}

int AcpClient::sendCall(std::string_view method, const Json& params, ResponseHandler onResult) {
    const int id = nextId_++;
    pending_.emplace(id, std::move(onResult));
    transport_.send(encodeRequest(Json(id), method, params));
    return id;
}

int AcpClient::initialize(const Json& clientCapabilities, ResponseHandler onResult) {
    Json params;
    params["protocolVersion"] = 1;
    params["clientCapabilities"] = clientCapabilities;
    return sendCall("initialize", params, std::move(onResult));
}

int AcpClient::newSession(const Json& params, ResponseHandler onResult) {
    return sendCall("session/new", params, std::move(onResult));
}

int AcpClient::prompt(std::string_view sessionId, const Json& contentBlocks, ResponseHandler onResult) {
    Json params;
    params["sessionId"] = std::string(sessionId);
    params["prompt"] = contentBlocks;
    return sendCall("session/prompt", params, std::move(onResult));
}

void AcpClient::cancel(std::string_view sessionId) {
    Json params;
    params["sessionId"] = std::string(sessionId);
    transport_.send(encodeNotification("session/cancel", params));
}

void AcpClient::respondPermission(const Json& requestId, std::string_view optionId) {
    Json result;
    result["outcome"] = "selected";
    result["optionId"] = std::string(optionId);
    transport_.send(encodeResponse(requestId, result));
}

void AcpClient::receiveBytes(std::string_view bytes) {
    buffer_.append(bytes);

    std::size_t start = 0;
    for (std::size_t i = 0; i < buffer_.size(); ++i) {
        if (buffer_[i] != '\n') {
            continue;
        }
        std::size_t end = i;
        if (end > start && buffer_[end - 1] == '\r') {
            --end; // tolerate CRLF
        }
        if (end > start) {
            dispatch(parseJsonRpc(std::string_view(buffer_).substr(start, end - start)));
        }
        start = i + 1;
    }
    buffer_.erase(0, start);
}

void AcpClient::dispatch(const JsonRpcMessage& msg) {
    switch (msg.kind) {
    case MessageKind::Response: {
        if (msg.id && msg.id->is_number_integer()) {
            const int id = msg.id->get<int>();
            const auto it = pending_.find(id);
            if (it != pending_.end()) {
                ResponseHandler handler = std::move(it->second);
                pending_.erase(it);
                if (handler) {
                    handler(msg.result, msg.isError ? &msg.error : nullptr);
                }
            }
        }
        break;
    }
    case MessageKind::Request: {
        if (msg.method == "session/request_permission" && onPermissionRequest) {
            PermissionRequest req;
            if (msg.id) {
                req.requestId = *msg.id;
            }
            if (msg.params.contains("sessionId")) {
                req.sessionId = msg.params["sessionId"].get<std::string>();
            }
            if (msg.params.contains("toolCall")) {
                req.toolCall = msg.params["toolCall"];
            }
            if (msg.params.contains("options") && msg.params["options"].is_array()) {
                for (const auto& opt : msg.params["options"]) {
                    PermissionOption po;
                    if (opt.contains("optionId")) po.optionId = opt["optionId"].get<std::string>();
                    if (opt.contains("name")) po.name = opt["name"].get<std::string>();
                    if (opt.contains("kind")) po.kind = opt["kind"].get<std::string>();
                    req.options.push_back(std::move(po));
                }
            }
            onPermissionRequest(req);
        }
        break;
    }
    case MessageKind::Notification: {
        if (msg.method == "session/update" && onSessionUpdate) {
            SessionUpdate update;
            if (msg.params.contains("sessionId")) {
                update.sessionId = msg.params["sessionId"].get<std::string>();
            }
            if (msg.params.contains("update")) {
                const Json& u = msg.params["update"];
                update.data = u;
                if (u.contains("sessionUpdate") && u["sessionUpdate"].is_string()) {
                    update.kind = updateKindFromString(u["sessionUpdate"].get<std::string>());
                }
            }
            onSessionUpdate(update);
        }
        break;
    }
    case MessageKind::Invalid:
        break;
    }
}

} // namespace maestro::acp
