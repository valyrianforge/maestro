#include "maestro/acp/ApprovalBroker.hpp"

#include <algorithm>

namespace maestro::acp {
namespace {

// Pick the option id matching a decision, preferring the "once" variant over
// the "always" variant. Returns nullopt if the request offers no suitable option.
std::optional<std::string> pickOption(const PermissionRequest& request, Decision decision) {
    const std::string primary = decision == Decision::Allow ? "allow_once" : "reject_once";
    const std::string fallback = decision == Decision::Allow ? "allow_always" : "reject_always";

    for (const auto& opt : request.options) {
        if (opt.kind == primary) return opt.optionId;
    }
    for (const auto& opt : request.options) {
        if (opt.kind == fallback) return opt.optionId;
    }
    return std::nullopt;
}

std::string keyOf(const Json& requestId) { return requestId.dump(); }

} // namespace

ApprovalBroker::ApprovalBroker(Responder responder) : responder_(std::move(responder)) {}

void ApprovalBroker::addRule(Rule rule) { rules_.push_back(std::move(rule)); }

void ApprovalBroker::handle(const PermissionRequest& request) {
    for (const auto& rule : rules_) {
        if (const std::optional<Decision> d = rule(request)) {
            apply(request, *d);
            return;
        }
    }
    pending_.emplace(keyOf(request.requestId), request);
    if (onApprovalNeeded) {
        onApprovalNeeded(request);
    }
}

bool ApprovalBroker::resolve(const Json& requestId, Decision decision) {
    const auto it = pending_.find(keyOf(requestId));
    if (it == pending_.end()) {
        return false;
    }
    const PermissionRequest request = it->second;
    pending_.erase(it);
    apply(request, decision);
    return true;
}

void ApprovalBroker::apply(const PermissionRequest& request, Decision decision) {
    if (const std::optional<std::string> optionId = pickOption(request, decision)) {
        if (responder_) {
            responder_(request.requestId, *optionId);
        }
    }
}

namespace rules {

ApprovalBroker::Rule allowWhenToolKindIn(std::vector<std::string> kinds) {
    return [kinds = std::move(kinds)](const PermissionRequest& r) -> std::optional<Decision> {
        if (!r.toolCall.contains("kind") || !r.toolCall["kind"].is_string()) {
            return std::nullopt;
        }
        const std::string kind = r.toolCall["kind"].get<std::string>();
        if (std::find(kinds.begin(), kinds.end(), kind) != kinds.end()) {
            return Decision::Allow;
        }
        return std::nullopt;
    };
}

ApprovalBroker::Rule denyWhenToolKindIn(std::vector<std::string> kinds) {
    return [kinds = std::move(kinds)](const PermissionRequest& r) -> std::optional<Decision> {
        if (!r.toolCall.contains("kind") || !r.toolCall["kind"].is_string()) {
            return std::nullopt;
        }
        const std::string kind = r.toolCall["kind"].get<std::string>();
        if (std::find(kinds.begin(), kinds.end(), kind) != kinds.end()) {
            return Decision::Deny;
        }
        return std::nullopt;
    };
}

} // namespace rules

} // namespace maestro::acp
