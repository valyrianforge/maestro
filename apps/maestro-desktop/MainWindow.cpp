#include "MainWindow.hpp"

#include <QComboBox>
#include <QDockWidget>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStatusBar>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTextCursor>
#include <QTextEdit>
#include <QSplitter>
#include <QVBoxLayout>
#include <QWidget>

#include "EngineController.hpp"
#include "GraphCanvas.hpp"

namespace maestro::desktop {

MainWindow::MainWindow(EngineController* controller, QWidget* parent)
    : QMainWindow(parent), controller_(controller) {
    buildUi();

    connect(runButton_, &QPushButton::clicked, this, &MainWindow::onRunClicked);
    connect(controller_, &EngineController::runStarted, this, &MainWindow::onRunStarted);
    connect(controller_, &EngineController::runFinished, this, &MainWindow::onRunFinished);
    connect(controller_, &EngineController::logMessage, this, &MainWindow::onLogMessage);
    connect(controller_, &EngineController::taskAdded, this, &MainWindow::onTaskAdded);
    connect(controller_, &EngineController::edgeAdded, this, &MainWindow::onEdgeAdded);
    connect(controller_, &EngineController::taskStateChanged, this,
            &MainWindow::onTaskStateChanged);
    connect(controller_, &EngineController::assistantText, this, &MainWindow::onAssistantText);
    connect(controller_, &EngineController::agentStatus, this, &MainWindow::onAgentStatus);
}

void MainWindow::buildUi() {
    setWindowTitle("Maestro — AI Orchestration Platform");
    resize(1180, 760);

    // --- Top command bar ---
    auto* bar = new QWidget(this);
    auto* barLayout = new QHBoxLayout(bar);
    barLayout->setContentsMargins(8, 8, 8, 8);
    promptEdit_ = new QLineEdit(bar);
    promptEdit_->setPlaceholderText("Enter a topic (pipeline) or a prompt (single)…");
    modeCombo_ = new QComboBox(bar);
    modeCombo_->addItem("Pipeline: research → draft → critique");
    modeCombo_->addItem("Fan-out (4 parallel agents)");
    modeCombo_->addItem("Single prompt");
    modeCombo_->addItem("Auto-plan (spawn subagents)");
    runButton_ = new QPushButton("Run", bar);
    runButton_->setDefault(true);
    barLayout->addWidget(new QLabel("Goal:", bar));
    barLayout->addWidget(promptEdit_, /*stretch=*/1);
    barLayout->addWidget(modeCombo_);
    barLayout->addWidget(runButton_);

    // --- Center: live agent graph (top) + streaming conversation (bottom) ---
    graph_ = new GraphCanvas(this);
    conversation_ = new QTextEdit(this);
    conversation_->setReadOnly(true);
    conversation_->setPlaceholderText("Agent output will stream here.");

    auto* split = new QSplitter(Qt::Vertical, this);
    split->addWidget(graph_);
    split->addWidget(conversation_);
    split->setStretchFactor(0, 3);
    split->setStretchFactor(1, 2);

    auto* center = new QWidget(this);
    auto* centerLayout = new QVBoxLayout(center);
    centerLayout->setContentsMargins(0, 0, 0, 0);
    centerLayout->addWidget(bar);
    centerLayout->addWidget(split, 1);
    setCentralWidget(center);

    // --- Left dock: agents + tasks ---
    agentsTable_ = new QTableWidget(0, 3, this);
    agentsTable_->setHorizontalHeaderLabels({"Agent", "Provider", "Status"});
    agentsTable_->horizontalHeader()->setStretchLastSection(true);
    agentsTable_->verticalHeader()->setVisible(false);
    agentsTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);

    tasksTable_ = new QTableWidget(0, 2, this);
    tasksTable_->setHorizontalHeaderLabels({"Task", "State"});
    tasksTable_->horizontalHeader()->setStretchLastSection(true);
    tasksTable_->verticalHeader()->setVisible(false);
    tasksTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);

    auto* leftPanel = new QWidget(this);
    auto* leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(4, 4, 4, 4);
    leftLayout->addWidget(new QLabel("Agents", leftPanel));
    leftLayout->addWidget(agentsTable_, 1);
    leftLayout->addWidget(new QLabel("Task Queue", leftPanel));
    leftLayout->addWidget(tasksTable_, 1);

    auto* leftDock = new QDockWidget("Fleet", this);
    leftDock->setWidget(leftPanel);
    leftDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    addDockWidget(Qt::LeftDockWidgetArea, leftDock);

    // --- Bottom dock: live logs ---
    logs_ = new QPlainTextEdit(this);
    logs_->setReadOnly(true);
    logs_->setMaximumBlockCount(5000);
    auto* logDock = new QDockWidget("Live Logs", this);
    logDock->setWidget(logs_);
    addDockWidget(Qt::BottomDockWidgetArea, logDock);

    // --- Status bar ---
    statusLabel_ = new QLabel("Ready.", this);
    statusBar()->addWidget(statusLabel_);

    // Dark IDE-ish theme.
    setStyleSheet(R"(
        QWidget { background:#1e1f22; color:#e6e6e6; font-size:13px; }
        QLineEdit, QComboBox, QTextEdit, QPlainTextEdit, QTableWidget {
            background:#2b2d31; border:1px solid #3a3d42; border-radius:4px; }
        QPushButton { background:#4c6ef5; color:white; border:none; padding:6px 16px;
                      border-radius:4px; font-weight:600; }
        QPushButton:disabled { background:#3a3d42; color:#888; }
        QHeaderView::section { background:#2b2d31; border:none; padding:4px; }
        QDockWidget::title { background:#26282c; padding:4px; }
    )");
}

void MainWindow::onRunClicked() {
    const QString text = promptEdit_->text().trimmed();
    if (text.isEmpty() || controller_->isRunning()) {
        return;
    }
    conversation_->clear();
    graph_->clearGraph();
    tasksTable_->setRowCount(0);
    agentsTable_->setRowCount(0);
    taskRow_.clear();
    agentRow_.clear();
    lastStreamTask_.clear();

    controller_->start(modeCombo_->currentIndex(), text);
}

void MainWindow::onRunStarted() {
    runButton_->setEnabled(false);
    statusLabel_->setText("Running…");
}

void MainWindow::onRunFinished(int succeeded, int failed, int blocked) {
    runButton_->setEnabled(true);
    statusLabel_->setText(QString("Done — %1 succeeded, %2 failed, %3 blocked.")
                              .arg(succeeded)
                              .arg(failed)
                              .arg(blocked));
}

void MainWindow::onLogMessage(const QString& line) { logs_->appendPlainText(line); }

void MainWindow::onTaskAdded(const QString& name, const QString& provider) {
    if (taskRow_.contains(name)) {
        return;
    }
    const int row = tasksTable_->rowCount();
    tasksTable_->insertRow(row);
    tasksTable_->setItem(row, 0, new QTableWidgetItem(name + "  (" + provider + ")"));
    tasksTable_->setItem(row, 1, new QTableWidgetItem("pending"));
    taskRow_.insert(name, row);
    graph_->addNode(name, provider);
}

void MainWindow::onEdgeAdded(const QString& from, const QString& to) { graph_->addEdge(from, to); }

void MainWindow::onTaskStateChanged(const QString& name, const QString& state) {
    if (const auto it = taskRow_.constFind(name); it != taskRow_.constEnd()) {
        tasksTable_->setItem(it.value(), 1, new QTableWidgetItem(state));
    }
    graph_->setNodeState(name, state);
}

void MainWindow::onAssistantText(const QString& taskName, const QString& text) {
    if (taskName != lastStreamTask_) {
        conversation_->append(QString("\n=== [%1] ===\n").arg(taskName));
        lastStreamTask_ = taskName;
    }
    conversation_->moveCursor(QTextCursor::End);
    conversation_->insertPlainText(text);
    conversation_->moveCursor(QTextCursor::End);
}

void MainWindow::onAgentStatus(quint64 agentId, const QString& provider, const QString& status) {
    int row;
    if (const auto it = agentRow_.constFind(agentId); it != agentRow_.constEnd()) {
        row = it.value();
    } else {
        row = agentsTable_->rowCount();
        agentsTable_->insertRow(row);
        agentsTable_->setItem(row, 0, new QTableWidgetItem(QString("agent-%1").arg(agentId)));
        agentsTable_->setItem(row, 1, new QTableWidgetItem(provider));
        agentRow_.insert(agentId, row);
    }
    agentsTable_->setItem(row, 2, new QTableWidgetItem(status));
}

} // namespace maestro::desktop
