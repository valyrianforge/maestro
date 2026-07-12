#pragma once
#include <QString>
#include <QStringList>
#include <QWidget>

class QLabel;
class QListWidget;
class QPushButton;

namespace maestro::desktop {

// The human-in-the-loop approval surface (the "hybrid nucleus" gate).
//
// Top section: the plan the nucleus proposed (one row per worker) with
// Approve / Reject. Bottom section: a queue of per-action approval requests
// (a tool call awaiting a decision) with Allow / Deny for the selected one.
//
// Pure view: it emits decisions as signals and never touches the engine. The
// owner wires planApproved/planRejected/actionDecision to the ACP layer
// (PlanApprovalGate::approve/reject and ApprovalBroker::resolve).
class ApprovalPanel : public QWidget {
    Q_OBJECT
public:
    explicit ApprovalPanel(QWidget* parent = nullptr);

public slots:
    // Show a proposed plan; each entry is a human-readable worker description.
    void showPlan(const QString& summary, const QStringList& workers);
    void clearPlan();
    // Add a pending per-action approval. requestId round-trips back in actionDecision.
    void addPendingApproval(const QString& requestId, const QString& description);

signals:
    void planApproved();
    void planRejected();
    void actionDecision(const QString& requestId, bool allow);

private slots:
    void onApprovePlan();
    void onRejectPlan();
    void onAllow();
    void onDeny();

private:
    void decideSelected(bool allow);

    QLabel* planSummary_{nullptr};
    QListWidget* planWorkers_{nullptr};
    QPushButton* approveBtn_{nullptr};
    QPushButton* rejectBtn_{nullptr};
    QListWidget* pending_{nullptr};
    QPushButton* allowBtn_{nullptr};
    QPushButton* denyBtn_{nullptr};
};

} // namespace maestro::desktop
