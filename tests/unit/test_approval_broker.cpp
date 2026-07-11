#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "maestro/acp/ApprovalBroker.hpp"

using namespace maestro::acp;

namespace {

// Build a permission request with the standard four options.
PermissionRequest makeRequest(int id, const std::string& toolKind) {
    PermissionRequest r;
    r.requestId = Json(id);
    r.sessionId = "sess-1";
    r.toolCall = Json{{"kind", toolKind}};
    r.options = {
        {"allow", "Allow", "allow_once"},
        {"allow-all", "Always allow", "allow_always"},
        {"deny", "Deny", "reject_once"},
    };
    return r;
}

// Records what the broker answered the agent.
struct Recorder {
    std::vector<std::pair<std::string, std::string>> answers; // (requestId.dump(), optionId)
    ApprovalBroker::Responder responder() {
        return [this](const Json& id, const std::string& optionId) {
            answers.emplace_back(id.dump(), optionId);
        };
    }
};

} // namespace

TEST_CASE("an allow rule auto-approves without asking a human", "[approval]") {
    Recorder rec;
    ApprovalBroker broker(rec.responder());
    broker.addRule(rules::allowWhenToolKindIn({"read"}));

    bool asked = false;
    broker.onApprovalNeeded = [&](const PermissionRequest&) { asked = true; };

    broker.handle(makeRequest(1, "read"));

    REQUIRE_FALSE(asked);
    REQUIRE(broker.pendingCount() == 0);
    REQUIRE(rec.answers.size() == 1);
    REQUIRE(rec.answers[0].second == "allow"); // prefers allow_once
}

TEST_CASE("a deny rule auto-rejects", "[approval]") {
    Recorder rec;
    ApprovalBroker broker(rec.responder());
    broker.addRule(rules::denyWhenToolKindIn({"execute"}));

    broker.handle(makeRequest(2, "execute"));

    REQUIRE(rec.answers.size() == 1);
    REQUIRE(rec.answers[0].second == "deny");
}

TEST_CASE("an unmatched request waits for a human, then resolve() answers", "[approval]") {
    Recorder rec;
    ApprovalBroker broker(rec.responder());
    broker.addRule(rules::allowWhenToolKindIn({"read"}));

    PermissionRequest captured;
    broker.onApprovalNeeded = [&](const PermissionRequest& r) { captured = r; };

    broker.handle(makeRequest(7, "edit")); // no rule matches "edit"

    REQUIRE(broker.pendingCount() == 1);
    REQUIRE(rec.answers.empty()); // nothing answered yet
    REQUIRE(captured.sessionId == "sess-1");

    const bool ok = broker.resolve(Json(7), Decision::Allow);
    REQUIRE(ok);
    REQUIRE(broker.pendingCount() == 0);
    REQUIRE(rec.answers.size() == 1);
    REQUIRE(rec.answers[0].second == "allow");
}

TEST_CASE("resolving an unknown or already-resolved id returns false", "[approval]") {
    Recorder rec;
    ApprovalBroker broker(rec.responder());

    REQUIRE_FALSE(broker.resolve(Json(999), Decision::Allow));

    broker.handle(makeRequest(3, "edit"));
    REQUIRE(broker.resolve(Json(3), Decision::Deny));
    REQUIRE_FALSE(broker.resolve(Json(3), Decision::Allow)); // second time: gone
}

TEST_CASE("first matching rule wins", "[approval]") {
    Recorder rec;
    ApprovalBroker broker(rec.responder());
    broker.addRule(rules::denyWhenToolKindIn({"edit"}));
    broker.addRule(rules::allowWhenToolKindIn({"edit"})); // shadowed by the deny above

    broker.handle(makeRequest(4, "edit"));

    REQUIRE(rec.answers.size() == 1);
    REQUIRE(rec.answers[0].second == "deny");
}
