#include "ApprovalPanel.hpp"

#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

namespace maestro::desktop {

namespace {
constexpr int kRequestIdRole = Qt::UserRole + 1;
} // namespace

ApprovalPanel::ApprovalPanel(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);

    // --- Proposed plan ---
    auto* planBox = new QGroupBox("Proposed plan", this);
    auto* planLayout = new QVBoxLayout(planBox);
    planSummary_ = new QLabel("No plan proposed.", planBox);
    planSummary_->setWordWrap(true);
    planWorkers_ = new QListWidget(planBox);
    approveBtn_ = new QPushButton("Approve", planBox);
    rejectBtn_ = new QPushButton("Reject", planBox);
    approveBtn_->setEnabled(false);
    rejectBtn_->setEnabled(false);
    auto* planButtons = new QHBoxLayout();
    planButtons->addWidget(approveBtn_);
    planButtons->addWidget(rejectBtn_);
    planLayout->addWidget(planSummary_);
    planLayout->addWidget(planWorkers_, 1);
    planLayout->addLayout(planButtons);

    // --- Pending per-action approvals ---
    auto* actionBox = new QGroupBox("Actions awaiting approval", this);
    auto* actionLayout = new QVBoxLayout(actionBox);
    pending_ = new QListWidget(actionBox);
    allowBtn_ = new QPushButton("Allow", actionBox);
    denyBtn_ = new QPushButton("Deny", actionBox);
    auto* actionButtons = new QHBoxLayout();
    actionButtons->addWidget(allowBtn_);
    actionButtons->addWidget(denyBtn_);
    actionLayout->addWidget(pending_, 1);
    actionLayout->addLayout(actionButtons);

    layout->addWidget(planBox, 1);
    layout->addWidget(actionBox, 1);

    connect(approveBtn_, &QPushButton::clicked, this, &ApprovalPanel::onApprovePlan);
    connect(rejectBtn_, &QPushButton::clicked, this, &ApprovalPanel::onRejectPlan);
    connect(allowBtn_, &QPushButton::clicked, this, &ApprovalPanel::onAllow);
    connect(denyBtn_, &QPushButton::clicked, this, &ApprovalPanel::onDeny);
}

void ApprovalPanel::showPlan(const QString& summary, const QStringList& workers) {
    planSummary_->setText(summary);
    planWorkers_->clear();
    planWorkers_->addItems(workers);
    approveBtn_->setEnabled(true);
    rejectBtn_->setEnabled(true);
}

void ApprovalPanel::clearPlan() {
    planSummary_->setText("No plan proposed.");
    planWorkers_->clear();
    approveBtn_->setEnabled(false);
    rejectBtn_->setEnabled(false);
}

void ApprovalPanel::addPendingApproval(const QString& requestId, const QString& description) {
    auto* item = new QListWidgetItem(description, pending_);
    item->setData(kRequestIdRole, requestId);
}

void ApprovalPanel::onApprovePlan() {
    clearPlan();
    emit planApproved();
}

void ApprovalPanel::onRejectPlan() {
    clearPlan();
    emit planRejected();
}

void ApprovalPanel::onAllow() { decideSelected(true); }
void ApprovalPanel::onDeny() { decideSelected(false); }

void ApprovalPanel::decideSelected(bool allow) {
    QListWidgetItem* item = pending_->currentItem();
    if (item == nullptr) {
        return;
    }
    const QString requestId = item->data(kRequestIdRole).toString();
    emit actionDecision(requestId, allow);
    delete pending_->takeItem(pending_->row(item));
}

} // namespace maestro::desktop
