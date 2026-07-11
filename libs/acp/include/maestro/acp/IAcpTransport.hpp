#pragma once
#include <string_view>

namespace maestro::acp {

// The byte sink an AcpClient writes outgoing JSON-RPC frames to. Kept as a
// narrow, fakeable seam: the real implementation writes to an agent
// subprocess's stdin (via the process backend); the test fake records frames.
//
// Receiving is push-based and lives on AcpClient::receiveBytes(), mirroring the
// process backend's stdout-callback model, so this interface is send-only.
class IAcpTransport {
public:
    virtual ~IAcpTransport() = default;

    // Send one JSON-RPC frame. The implementation is responsible for appending
    // newline framing before it reaches the peer.
    virtual void send(std::string_view jsonLine) = 0;
};

} // namespace maestro::acp
