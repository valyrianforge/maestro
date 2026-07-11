#include "maestro/acp/PlanApprovalGate.hpp"

namespace maestro::acp {

void PlanApprovalGate::propose(const Plan& plan) {
    pending_ = plan;
    if (onPlanProposed) {
        onPlanProposed(plan);
    }
}

bool PlanApprovalGate::approve() {
    if (!pending_) {
        return false;
    }
    const Plan plan = *pending_;
    pending_.reset();
    if (onApproved) {
        onApproved(plan);
    }
    return true;
}

bool PlanApprovalGate::approve(const Plan& edited) {
    if (!pending_) {
        return false;
    }
    pending_.reset();
    if (onApproved) {
        onApproved(edited);
    }
    return true;
}

bool PlanApprovalGate::reject() {
    if (!pending_) {
        return false;
    }
    pending_.reset();
    if (onRejected) {
        onRejected();
    }
    return true;
}

} // namespace maestro::acp
