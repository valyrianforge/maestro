#pragma once
#include <QObject>
#include <QString>

#include <atomic>
#include <thread>

namespace maestro::desktop {

// Bridges the (synchronous, thread-blocking) orchestration engine to the Qt UI.
// A run executes on a background std::thread; every orchestrator event and
// streamed token is re-emitted as a Qt signal, delivered to the UI thread via
// queued connections. The UI never touches the engine directly — it observes
// these signals, matching the "UI observes Core via events" architecture.
class EngineController : public QObject {
    Q_OBJECT
public:
    explicit EngineController(QObject* parent = nullptr);
    ~EngineController() override;

    [[nodiscard]] bool isRunning() const { return running_.load(); }

public slots:
    // Runs the 3-stage pipeline (research -> draft -> critique) on a topic.
    void runPipeline(const QString& topic);
    // Runs a single prompt as a one-node graph.
    void runSingle(const QString& prompt);

signals:
    void runStarted();
    void runFinished(int succeeded, int failed, int blocked);
    void logMessage(const QString& line);
    void taskAdded(const QString& name, const QString& provider);
    void taskStateChanged(const QString& name, const QString& state);
    void assistantText(const QString& taskName, const QString& text);
    void agentStatus(quint64 agentId, const QString& provider, const QString& status);

private:
    void startRun(bool pipeline, QString text);

    std::thread worker_;
    std::atomic<bool> running_{false};
};

} // namespace maestro::desktop
