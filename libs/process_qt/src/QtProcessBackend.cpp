#include "maestro/process_qt/QtProcessBackend.hpp"

#include <QProcess>
#include <QProcessEnvironment>
#include <QString>
#include <QStringList>

#include <algorithm>

namespace maestro::process {
namespace {

std::string_view toView(const QByteArray& bytes) {
    return std::string_view(bytes.constData(), static_cast<std::size_t>(bytes.size()));
}

} // namespace

QtProcessBackend::QtProcessBackend() = default;
QtProcessBackend::~QtProcessBackend() {
    for (Child& c : children_) {
        if (c.proc) {
            c.proc->kill();
            c.proc->waitForFinished(1000);
        }
    }
}

void QtProcessBackend::setObserver(Observer* observer) { observer_ = observer; }

void QtProcessBackend::start(ProcessHandle handle, const ProcessSpec& spec) {
    auto proc = std::make_unique<QProcess>();

    QProcessEnvironment env = spec.inheritParentEnv ? QProcessEnvironment::systemEnvironment()
                                                    : QProcessEnvironment();
    for (const auto& [key, value] : spec.env) {
        env.insert(QString::fromStdString(key), QString::fromStdString(value));
    }
    proc->setProcessEnvironment(env);
    if (spec.workingDirectory) {
        proc->setWorkingDirectory(QString::fromStdString(*spec.workingDirectory));
    }

    QStringList args;
    for (const auto& a : spec.args) {
        args << QString::fromStdString(a);
    }
    proc->setProgram(QString::fromStdString(spec.program));
    proc->setArguments(args);
    proc->start();
    proc->closeWriteChannel(); // headless: prompt is in argv, feed stdin EOF

    children_.push_back(Child{handle, std::move(proc), false});
}

void QtProcessBackend::write(ProcessHandle /*handle*/, std::string_view /*data*/) {
    // Stdin is closed on start (headless prompt-via-argv), so writes are ignored.
}

void QtProcessBackend::kill(ProcessHandle handle) {
    const auto it = std::find_if(children_.begin(), children_.end(),
                                 [&](const Child& c) { return c.handle == handle; });
    if (it == children_.end()) {
        return;
    }
    it->intentionalKill = true;
    if (it->proc) {
        it->proc->terminate();
    }
}

void QtProcessBackend::drain(Child& child) {
    const QByteArray out = child.proc->readAllStandardOutput();
    if (!out.isEmpty() && observer_) {
        observer_->onStdout(child.handle, toView(out));
    }
    const QByteArray err = child.proc->readAllStandardError();
    if (!err.isEmpty() && observer_) {
        observer_->onStderr(child.handle, toView(err));
    }
}

bool QtProcessBackend::processEvents(int timeoutMs) {
    if (children_.empty()) {
        return false;
    }

    // Block on the first live process to avoid busy-spinning, then service all.
    if (children_.front().proc) {
        children_.front().proc->waitForReadyRead(timeoutMs);
    }

    for (Child& c : children_) {
        drain(c);
    }

    std::vector<ProcessHandle> toReap;
    for (const Child& c : children_) {
        if (!c.proc || c.proc->state() == QProcess::NotRunning) {
            toReap.push_back(c.handle);
        }
    }
    for (const ProcessHandle h : toReap) {
        reapByHandle(h);
    }

    return !children_.empty();
}

void QtProcessBackend::reapByHandle(ProcessHandle handle) {
    const auto it = std::find_if(children_.begin(), children_.end(),
                                 [&](const Child& c) { return c.handle == handle; });
    if (it == children_.end()) {
        return;
    }
    QProcess* proc = it->proc.get();
    const bool killed = it->intentionalKill;

    // Ensure the process is fully reaped and any last output is flushed.
    proc->waitForFinished(0);
    drain(*it);

    ProcessExit exit{core::ExitReason::Crashed, -1};
    if (killed) {
        exit = ProcessExit{core::ExitReason::Killed, -1};
    } else if (proc->error() == QProcess::FailedToStart) {
        exit = ProcessExit{core::ExitReason::Crashed, -1};
    } else if (proc->exitStatus() == QProcess::NormalExit) {
        exit = ProcessExit{core::ExitReason::Exited, proc->exitCode()};
    }

    children_.erase(it); // erase before notifying (a restart may push a new child)
    if (observer_) {
        observer_->onExit(handle, exit);
    }
}

void QtProcessBackend::runUntilIdle() {
    while (!children_.empty()) {
        processEvents(50);
    }
}

} // namespace maestro::process
