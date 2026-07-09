#include "maestro/runtime/ProcessTaskExecutor.hpp"

#include "maestro/process/ProcessManager.hpp"
#include "maestro/providers/NdjsonLineReader.hpp"
#include "maestro/runtime/ResultCollector.hpp"

namespace maestro::runtime {

using core::ProcessExit;
using core::TaskChunk;
using core::TaskRequest;
using process::ProcessCallbacks;
using process::ProcessManager;
using process::RestartPolicy;
using providers::NdjsonLineReader;

ProcessTaskExecutor::ProcessTaskExecutor(const ProviderRegistry& registry,
                                         BackendFactory backendFactory, StreamHook onAssistantText)
    : registry_(registry),
      backendFactory_(std::move(backendFactory)),
      onAssistantText_(std::move(onAssistantText)) {}

TaskResult ProcessTaskExecutor::execute(const ExecRequest& request) {
    const auto provider = registry_.get(request.provider);
    if (!provider) {
        TaskResult miss;
        miss.success = false;
        miss.output = "no provider registered for \"" + request.provider.name + "\"";
        return miss;
    }

    TaskRequest treq;
    treq.prompt = request.prompt;
    treq.resume = request.resume;
    treq.workingDirectory = request.workingDirectory;
    const auto spec = provider->buildSpec(treq);

    auto backend = backendFactory_();
    ProcessManager manager(*backend);
    NdjsonLineReader reader;
    ResultCollector collector;
    ProcessExit exitStatus{core::ExitReason::Crashed, -1};
    bool done = false;

    auto handleLine = [&](const std::string& line) {
        if (const auto chunk = provider->parseFrame(line)) {
            if (chunk->kind == TaskChunk::Kind::AssistantText && onAssistantText_) {
                onAssistantText_(request, chunk->text);
            }
            collector.add(*chunk);
        }
    };

    manager.spawn(spec, RestartPolicy::none(),
                  ProcessCallbacks{
                      [&](std::string_view s) {
                          for (const auto& line : reader.feed(s)) {
                              handleLine(line);
                          }
                      },
                      [&](std::string_view s) { collector.addStderr(s); },
                      [&](ProcessExit e) {
                          exitStatus = e;
                          done = true;
                      }});

    while (!done) {
        backend->processEvents(200);
    }
    if (const auto tail = reader.flush()) {
        handleLine(*tail);
    }

    return collector.finalize(exitStatus.succeeded());
}

} // namespace maestro::runtime
