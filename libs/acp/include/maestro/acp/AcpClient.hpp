#pragma once
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "maestro/acp/IAcpTransport.hpp"
#include "maestro/acp/JsonRpc.hpp"

namespace maestro::acp {

// One streamed update the agent pushes during a turn (ACP `session/update`
// notification). `data` carries the raw `update` payload so callers can read
// message text, plan entries, tool-call details, etc. without this layer
// modelling every ACP field.
struct SessionUpdate {
    enum class Kind {
        AgentMessageChunk,
        AgentThoughtChunk,
        Plan,
        ToolCall,
        ToolCallUpdate,
        UsageUpdate,
        Unknown,
    };
    Kind kind = Kind::Unknown;
    std::string sessionId;
    Json data;
};

// One choice offered in a `session/request_permission` request.
struct PermissionOption {
    std::string optionId;
    std::string name;
    std::string kind; // allow_once | allow_always | reject_once | reject_always
};

// An agent-initiated request to approve/deny a tool call. `requestId` is the
// JSON-RPC id the client must echo back via respondPermission().
struct PermissionRequest {
    Json requestId;
    std::string sessionId;
    Json toolCall;
    std::vector<PermissionOption> options;
};

// Speaks the client half of the Agent Client Protocol over an IAcpTransport.
//
// Outgoing calls (initialize/newSession/prompt) are non-blocking: they write a
// JSON-RPC request and register a handler invoked when the matching response is
// fed back through receiveBytes(). Agent-initiated traffic (streamed updates
// and permission requests) is delivered via the public std::function hooks.
//
// The class is transport- and thread-agnostic: it only touches bytes, so it is
// fully unit-testable against a fake transport with no real subprocess.
class AcpClient {
public:
    using ResponseHandler = std::function<void(const Json& result, const Json* error)>;

    explicit AcpClient(IAcpTransport& transport);

    // --- Client -> agent calls. Each returns the numeric JSON-RPC id used. ---
    int initialize(const Json& clientCapabilities, ResponseHandler onResult);
    int newSession(const Json& params, ResponseHandler onResult);
    int prompt(std::string_view sessionId, const Json& contentBlocks, ResponseHandler onResult);

    // Cancel the in-flight turn for a session (fire-and-forget notification).
    void cancel(std::string_view sessionId);

    // Reply to a permission request the agent raised.
    void respondPermission(const Json& requestId, std::string_view optionId);

    // --- Agent -> client hooks (set by the owner). ---
    std::function<void(const SessionUpdate&)> onSessionUpdate;
    std::function<void(const PermissionRequest&)> onPermissionRequest;

    // Feed raw bytes read from the agent's stdout. Reassembles newline-framed
    // frames across arbitrary chunk boundaries and dispatches complete ones.
    void receiveBytes(std::string_view bytes);

private:
    int sendCall(std::string_view method, const Json& params, ResponseHandler onResult);
    void dispatch(const JsonRpcMessage& msg);

    IAcpTransport& transport_;
    std::string buffer_;
    int nextId_ = 1;
    std::unordered_map<int, ResponseHandler> pending_;
};

} // namespace maestro::acp
