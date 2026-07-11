#pragma once
#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "maestro/acp/AcpClient.hpp"

namespace maestro::acp {

// The allow/deny outcome for a tool call.
enum class Decision { Allow, Deny };

// Policy layer over ACP `session/request_permission`. Each incoming permission
// request is first matched against ordered auto-rules; if a rule fires, the
// broker answers the agent immediately. Otherwise the request is held pending
// and onApprovalNeeded fires so a human (or the nucleus) can decide via
// resolve(). This is the hybrid human-in-the-loop gate: safe/read-only actions
// can auto-approve while anything unrecognised waits for a person.
//
// Qt-free and transport-agnostic: it answers through a Responder callback
// (wired to AcpClient::respondPermission) so it is unit-testable in isolation.
class ApprovalBroker {
public:
    // Answers the agent: (jsonRpcRequestId, chosen optionId).
    using Responder = std::function<void(const Json& requestId, const std::string& optionId)>;
    // Returns a decision to apply, or nullopt to let later rules / the human decide.
    using Rule = std::function<std::optional<Decision>(const PermissionRequest&)>;

    explicit ApprovalBroker(Responder responder);

    // Append an auto-rule. Rules are evaluated in insertion order; first match wins.
    void addRule(Rule rule);

    // Handle one incoming permission request (wire AcpClient::onPermissionRequest here).
    void handle(const PermissionRequest& request);

    // Decide a previously-pending request. Returns false if it isn't pending
    // (e.g. already resolved, or auto-ruled).
    bool resolve(const Json& requestId, Decision decision);

    // Fired when no auto-rule matched and a human decision is required.
    std::function<void(const PermissionRequest&)> onApprovalNeeded;

    [[nodiscard]] std::size_t pendingCount() const { return pending_.size(); }

private:
    void apply(const PermissionRequest& request, Decision decision);

    Responder responder_;
    std::vector<Rule> rules_;
    std::unordered_map<std::string, PermissionRequest> pending_; // key: requestId.dump()
};

// Rule helpers.
namespace rules {

// Auto-allow when the tool call's "kind" is in the given set (e.g. {"read"}).
ApprovalBroker::Rule allowWhenToolKindIn(std::vector<std::string> kinds);
// Auto-deny when the tool call's "kind" is in the given set.
ApprovalBroker::Rule denyWhenToolKindIn(std::vector<std::string> kinds);

} // namespace rules

} // namespace maestro::acp
