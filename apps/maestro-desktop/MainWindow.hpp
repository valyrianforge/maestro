#pragma once
#include <QHash>
#include <QMainWindow>

class QComboBox;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QTableWidget;
class QTextEdit;
class QLabel;

namespace maestro::desktop {

class EngineController;
class GraphCanvas;
class ApprovalPanel;

// IDE-style main window: a prompt/topic bar, an agent list and task queue on the
// left, a streaming conversation view in the center, and a live log dock at the
// bottom. It is a pure observer of EngineController's signals.
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(EngineController* controller, QWidget* parent = nullptr);

private slots:
    void onRunClicked();
    void onRunStarted();
    void onRunFinished(int succeeded, int failed, int blocked);
    void onLogMessage(const QString& line);
    void onTaskAdded(const QString& name, const QString& provider);
    void onEdgeAdded(const QString& from, const QString& to);
    void onTaskStateChanged(const QString& name, const QString& state);
    void onAssistantText(const QString& taskName, const QString& text);
    void onAgentStatus(quint64 agentId, const QString& provider, const QString& status);
    // v2 interactive surfaces.
    void onNodeClicked(const QString& name);
    void onPlanApproved();
    void onPlanRejected();
    void onActionDecision(const QString& requestId, bool allow);

private:
    void buildUi();

    EngineController* controller_;

    QLineEdit* promptEdit_{nullptr};
    QComboBox* modeCombo_{nullptr};
    QPushButton* runButton_{nullptr};
    QTextEdit* conversation_{nullptr};
    GraphCanvas* graph_{nullptr};
    ApprovalPanel* approvals_{nullptr};
    QPlainTextEdit* logs_{nullptr};
    QTableWidget* agentsTable_{nullptr};
    QTableWidget* tasksTable_{nullptr};
    QLabel* statusLabel_{nullptr};

    QHash<QString, int> taskRow_;      // task name -> row in tasksTable_
    QHash<quint64, int> agentRow_;     // agent id  -> row in agentsTable_
    QString lastStreamTask_;           // to print a header when the task changes
};

} // namespace maestro::desktop
