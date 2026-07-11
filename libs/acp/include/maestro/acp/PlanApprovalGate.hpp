#pragma once
#include <functional>
#include <optional>

#include "maestro/acp/Plan.hpp"

namespace maestro::acp {

// Plan-level human approval gate — distinct from the per-action ApprovalBroker.
// The nucleus proposes an entire fan-out; the human approves it as-is, approves
// an edited version, or rejects it, before any worker is spawned. Only one plan
// is pending at a time.
class PlanApprovalGate {
public:
    // Called when a plan is awaiting a human decision.
    std::function<void(const Plan&)> onPlanProposed;
    // Called with the final (possibly edited) plan when approved.
    std::function<void(const Plan&)> onApproved;
    // Called when a pending plan is rejected.
    std::function<void()> onRejected;

    // Submit a plan for approval. Replaces any currently-pending plan.
    void propose(const Plan& plan);

    // Approve the pending plan as-is. Returns false if none is pending.
    bool approve();
    // Approve, substituting an edited plan. Returns false if none is pending.
    bool approve(const Plan& edited);
    // Reject the pending plan. Returns false if none is pending.
    bool reject();

    [[nodiscard]] bool hasPending() const { return pending_.has_value(); }

private:
    std::optional<Plan> pending_;
};

} // namespace maestro::acp
