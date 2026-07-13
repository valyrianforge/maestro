#pragma once
#include <string>
#include <string_view>

#include "maestro/acp/IAcpTransport.hpp"
#include "maestro/process/ProcessManager.hpp"

namespace maestro::runtime {

// Bridges an AcpClient to a real agent subprocess: outgoing JSON-RPC frames are
// written (newline-framed) to the child's stdin via ProcessManager. Incoming
// bytes flow the other way — the owner wires the process's onStdout callback to
// AcpClient::receiveBytes when spawning.
//
// Construct, spawn the agent, then bind() the returned handle before use.
class ProcessAcpTransport final : public acp::IAcpTransport {
public:
    explicit ProcessAcpTransport(process::ProcessManager& manager) : manager_(manager) {}

    void bind(process::ProcessHandle handle) { handle_ = handle; }

    void send(std::string_view jsonLine) override {
        std::string framed(jsonLine);
        framed.push_back('\n');
        manager_.writeStdin(handle_, framed);
    }

private:
    process::ProcessManager& manager_;
    process::ProcessHandle handle_{};
};

} // namespace maestro::runtime
