#pragma once
#include <QObject>
#include <QString>

#include <atomic>
#include <thread>

namespace maestro::desktop {

// Bridges the (synchronous, thread-blocking) orchestration engine to the Qt UI.
// A run executes on a background std::thread; every orchestrator event and
// streamed token is re-emitted as a Qt signal, delivered to the UI thread via
// queued connections. The UI never touches the engine directly.
class EngineController : public QObject {
    Q_OBJECT
public:
    // Matches the mode combo order in MainWindow.
    enum Mode { Pipeline = 0, FanOut = 1, Single = 2, AutoPlan = 3 };

    explicit EngineController(QObject* parent = nullptr);
    ~EngineController() override;

    [[nodiscard]] bool isRunning() const { return running_.load(); }

public slots:
    void start(int mode, const QString& text);

signals:
    void runStarted();
    void runFinished(int succeeded, int failed, int blocked);
    void logMessage(const QString& line);
    void taskAdded(const QString& name, const QString& provider);
    void edgeAdded(const QString& from, const QString& to);
    void taskStateChanged(const QString& name, const QString& state);
    void assistantText(const QString& taskName, const QString& text);
    void agentStatus(quint64 agentId, const QString& provider, const QString& status);

private:
    std::thread worker_;
    std::atomic<bool> running_{false};
};

} // namespace maestro::desktop
