// Standalone verification that QtProcessBackend (the cross-platform backend the
// packaged app and the Windows build use) actually spawns a process and streams
// output through the full runtime path. Runs `echo` via GenericCliProvider.
#include <QCoreApplication>

#include <iostream>
#include <memory>

#include "maestro/process_qt/QtProcessBackend.hpp"
#include "maestro/providers/GenericCliProvider.hpp"
#include "maestro/runtime/ProcessTaskExecutor.hpp"
#include "maestro/runtime/ProviderRegistry.hpp"

using namespace maestro;

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);

    runtime::ProviderRegistry registry;
    registry.add(std::make_shared<providers::GenericCliProvider>(
        providers::GenericCliProvider::Config{core::ProviderId{"echo"}, "echo", {"{{prompt}}"},
                                              core::Capabilities{}}));

    runtime::ProcessTaskExecutor executor(
        registry, []() { return std::make_unique<process::QtProcessBackend>(); });

    orchestrator::ExecRequest req;
    req.provider = core::ProviderId{"echo"};
    req.prompt = "qt-backend-hello";
    const auto result = executor.execute(req);

    std::cout << "QtProcessBackend smoke: success=" << result.success << " output=["
              << result.output << "]\n";
    const bool ok = result.success && result.output.find("qt-backend-hello") != std::string::npos;
    std::cout << (ok ? "PASS\n" : "FAIL\n");
    return ok ? 0 : 1;
}
