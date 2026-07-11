#pragma once
#include <string>
#include <vector>

#include "maestro/acp/IAcpTransport.hpp"

namespace maestro::acp {

// Test double: records every frame the client sends so tests can assert on the
// outgoing wire traffic. Reusable across milestones' tests, mirroring the
// FakeProcessBackend pattern.
class FakeAcpTransport : public IAcpTransport {
public:
    void send(std::string_view jsonLine) override { sent.emplace_back(jsonLine); }

    [[nodiscard]] bool empty() const { return sent.empty(); }
    [[nodiscard]] const std::string& last() const { return sent.back(); }

    std::vector<std::string> sent;
};

} // namespace maestro::acp
