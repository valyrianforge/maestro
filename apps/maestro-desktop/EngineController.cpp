#include "EngineController.hpp"

#include <memory>
#include <string>

#include "maestro/orchestrator/Orchestrator.hpp"
#include "maestro/providers/ClaudeProvider.hpp"
#include "maestro/runtime/ProcessTaskExecutor.hpp"
#include "maestro/runtime/ProviderRegistry.hpp"

namespace maestro::desktop {

namespace orch = maestro::orchestrator;
namespace rt = maestro::runtime;
using maestro::core::ProviderId;
using maestro::providers::ClaudeProvider;

EngineController::EngineController(QObject* parent) : QObject(parent) {}

EngineController::~EngineController() {
    if (worker_.joinable()) {
        worker_.join();
    }
}

void EngineController::runPipeline(const QString& topic) { startRun(true, topic); }
void EngineController::runSingle(const QString& prompt) { startRun(false, prompt); }

void EngineController::startRun(bool pipeline, QString text) {
    bool expected = false;
    if (!running_.compare_exchange_strong(expected, true)) {
        return; // a run is already in progress
    }
    if (worker_.joinable()) {
        worker_.join(); // reap the previous finished run
    }

    worker_ = std::thread([this, pipeline, text]() {
        rt::ProviderRegistry registry;
        registry.add(std::make_shared<ClaudeProvider>());

        orch::TaskGraph graph;
        const std::string t = text.toStdString();

        if (pipeline) {
            orch::Task research;
            research.name = "research";
            research.provider = ProviderId{"claude"};
            research.prompt = "List 3 concise, factual bullet points about: " + t;
            const auto rid = graph.addTask(research);

            orch::Task draft;
            draft.name = "draft";
            draft.provider = ProviderId{"claude"};
            draft.prompt = "Using the research context, write a 3-sentence explanation of: " + t;
            const auto did = graph.addTask(draft);
            graph.addDependency(did, rid);

            orch::Task critique;
            critique.name = "critique";
            critique.provider = ProviderId{"claude"};
            critique.prompt = "Critique the draft above for accuracy and clarity in 2 short bullets.";
            const auto cid = graph.addTask(critique);
            graph.addDependency(cid, did);
        } else {
            orch::Task single;
            single.name = "response";
            single.provider = ProviderId{"claude"};
            single.prompt = t;
            graph.addTask(single);
        }

        // Announce the planned tasks up front so the UI can list them.
        for (std::size_t i = 1; i <= graph.size(); ++i) {
            const orch::Task& task = graph.at(maestro::core::TaskId{i});
            emit taskAdded(QString::fromStdString(task.name),
                           QString::fromStdString(task.provider.name));
        }

        orch::WorkspaceManager workspace;
        orch::AgentManager agents;

        std::string currentTask;
        rt::ProcessTaskExecutor executor(
            registry, [this, &currentTask](const orch::ExecRequest&, std::string_view chunk) {
                emit assistantText(QString::fromStdString(currentTask),
                                   QString::fromUtf8(chunk.data(),
                                                     static_cast<int>(chunk.size())));
            });

        orch::Orchestrator orchestrator(graph, executor, workspace, agents);
        orchestrator.setObserver([this, &graph, &currentTask](const orch::OrchestratorEvent& e) {
            using T = orch::OrchestratorEvent::Type;
            const QString name = QString::fromStdString(graph.at(e.task).name);
            const QString provider = QString::fromStdString(graph.at(e.task).provider.name);
            switch (e.type) {
            case T::TaskStarted:
                currentTask = graph.at(e.task).name;
                emit taskStateChanged(name, "running");
                emit agentStatus(e.agent.value(), provider, "busy");
                emit logMessage("Task '" + name + "' started on agent " +
                                QString::number(e.agent.value()));
                break;
            case T::TaskSucceeded:
                emit taskStateChanged(name, "succeeded");
                emit agentStatus(e.agent.value(), provider, "idle");
                emit logMessage("Task '" + name + "' succeeded");
                break;
            case T::TaskFailed:
                emit taskStateChanged(name, "failed");
                emit agentStatus(e.agent.value(), provider, "idle");
                emit logMessage("Task '" + name + "' FAILED: " +
                                QString::fromStdString(e.detail));
                break;
            case T::TaskBlocked:
                emit taskStateChanged(name, "blocked");
                break;
            }
        });

        emit runStarted();
        emit logMessage(pipeline ? "Starting pipeline: research -> draft -> critique"
                                 : "Starting single prompt");
        const orch::RunReport report = orchestrator.run();
        emit logMessage("Run complete.");

        running_.store(false);
        emit runFinished(report.succeeded, report.failed, report.blocked);
    });
}

} // namespace maestro::desktop
