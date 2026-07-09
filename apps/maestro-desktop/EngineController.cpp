#include "EngineController.hpp"

#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "maestro/orchestrator/Orchestrator.hpp"
#include "maestro/process_qt/QtProcessBackend.hpp"
#include "maestro/providers/ClaudeProvider.hpp"
#include "maestro/runtime/ProcessTaskExecutor.hpp"
#include "maestro/runtime/ProviderRegistry.hpp"

namespace maestro::desktop {

namespace orch = maestro::orchestrator;
namespace rt = maestro::runtime;
using maestro::core::ProviderId;
using maestro::providers::ClaudeProvider;

namespace {

orch::Task claudeTask(std::string name, std::string prompt) {
    orch::Task t;
    t.name = std::move(name);
    t.provider = ProviderId{"claude"};
    t.prompt = std::move(prompt);
    return t;
}

// Parse a planner's bullet/numbered list into subtask descriptions.
std::vector<std::string> parsePlan(const std::string& text) {
    std::vector<std::string> items;
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        std::size_t i = line.find_first_not_of(" \t");
        if (i == std::string::npos) {
            continue;
        }
        const char c = line[i];
        if (c == '-' || c == '*' || (c >= '0' && c <= '9')) {
            // strip leading marker chars and punctuation
            std::size_t start = i;
            while (start < line.size() &&
                   (line[start] == '-' || line[start] == '*' || line[start] == '.' ||
                    line[start] == ')' || (line[start] >= '0' && line[start] <= '9') ||
                    line[start] == ' ')) {
                ++start;
            }
            std::string item = line.substr(start);
            if (!item.empty() && item.size() > 3) {
                items.push_back(item);
            }
        }
        if (items.size() >= 5) {
            break;
        }
    }
    return items;
}

} // namespace

EngineController::EngineController(QObject* parent) : QObject(parent) {}

EngineController::~EngineController() {
    if (worker_.joinable()) {
        worker_.join();
    }
}

void EngineController::start(int mode, const QString& text) {
    bool expected = false;
    if (!running_.compare_exchange_strong(expected, true)) {
        return;
    }
    if (worker_.joinable()) {
        worker_.join();
    }

    worker_ = std::thread([this, mode, text]() {
        rt::ProviderRegistry registry;
        registry.add(std::make_shared<ClaudeProvider>());

        const std::string t = text.toStdString();
        orch::TaskGraph graph;
        orch::SchedulerConfig config;
        bool planner = false;

        if (mode == Pipeline) {
            const auto r = graph.addTask(
                claudeTask("research", "List 3 concise, factual bullet points about: " + t));
            const auto d = graph.addTask(claudeTask(
                "draft", "Using the research context, write a 3-sentence explanation of: " + t));
            graph.addDependency(d, r);
            const auto c = graph.addTask(claudeTask(
                "critique", "Critique the draft above for accuracy and clarity in 2 bullets."));
            graph.addDependency(c, d);
        } else if (mode == FanOut) {
            for (int i = 0; i < 4; ++i) {
                graph.addTask(claudeTask("worker-" + std::to_string(i + 1),
                                         t + " (answer in one short sentence)"));
            }
            config.maxConcurrency = 4;
        } else if (mode == AutoPlan) {
            graph.addTask(claudeTask(
                "planner", "Break this goal into 2-4 independent subtasks. Output ONLY a bullet "
                           "list, one subtask per line starting with '- '. Goal: " + t));
            config.maxConcurrency = 4;
            planner = true;
        } else { // Single
            graph.addTask(claudeTask("response", t));
        }

        // Announce initial tasks + their dependency edges up front.
        for (std::size_t i = 1; i <= graph.size(); ++i) {
            const orch::Task& task = graph.at(maestro::core::TaskId{i});
            emit taskAdded(QString::fromStdString(task.name),
                           QString::fromStdString(task.provider.name));
            for (const auto dep : task.dependencies) {
                emit edgeAdded(QString::fromStdString(graph.at(dep).name),
                               QString::fromStdString(task.name));
            }
        }

        orch::WorkspaceManager workspace;
        orch::AgentManager agents;

        std::string currentTask;
        rt::ProcessTaskExecutor executor(
            registry,
            []() { return std::make_unique<maestro::process::QtProcessBackend>(); },
            [this, &currentTask](const orch::ExecRequest&, std::string_view chunk) {
                emit assistantText(QString::fromStdString(currentTask),
                                   QString::fromUtf8(chunk.data(),
                                                     static_cast<qsizetype>(chunk.size())));
            });

        orch::Orchestrator orchestrator(graph, executor, workspace, agents, config);

        if (planner) {
            orchestrator.setExpander(
                [](const orch::Task& parent, const orch::TaskResult& result) {
                    std::vector<orch::Task> children;
                    if (parent.name != "planner") {
                        return children;
                    }
                    int idx = 1;
                    for (const auto& sub : parsePlan(result.output)) {
                        children.push_back(claudeTask("subagent-" + std::to_string(idx++),
                                                      "Complete this subtask concisely: " + sub));
                    }
                    return children;
                });
        }

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
            case T::TaskRetrying:
                emit taskStateChanged(name, "retrying");
                emit logMessage("Task '" + name + "' retrying");
                break;
            case T::TaskBlocked:
                emit taskStateChanged(name, "blocked");
                break;
            case T::TaskSpawned: {
                const QString parent = QString::fromStdString(e.detail);
                emit taskAdded(name, provider);
                emit edgeAdded(parent, name);
                emit taskStateChanged(name, "pending");
                emit logMessage("Spawned subagent '" + name + "' from '" + parent + "'");
                break;
            }
            }
        });

        emit runStarted();
        emit logMessage(planner ? "Planning… (agent will spawn subagents)" : "Starting run");
        const orch::RunReport report = orchestrator.run();
        running_.store(false);
        emit logMessage("Run complete.");
        emit runFinished(report.succeeded, report.failed, report.blocked);
    });
}

} // namespace maestro::desktop
