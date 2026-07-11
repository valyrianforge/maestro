#pragma once
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "maestro/acp/AcpClient.hpp"
#include "maestro/acp/SessionController.hpp"

namespace maestro::acp {

// Metadata describing one live session's place in the fleet.
struct SessionInfo {
    std::string sessionId;
    std::string role;                     // "nucleus" | "worker" | free-form label
    std::optional<std::string> parentId;  // the "string" tying an electron to its nucleus
};

// Registry of all live sessions (the nucleus and its workers) and their
// controllers. Owns the SessionControllers and exposes the parent/child
// structure the live agent graph renders.
class SessionManager {
public:
    // Register a session and create its controller over the given client.
    SessionController& add(const SessionInfo& info, AcpClient& client);

    [[nodiscard]] SessionController* controller(const std::string& sessionId);
    [[nodiscard]] const SessionInfo* info(const std::string& sessionId) const;
    [[nodiscard]] std::vector<SessionInfo> sessions() const;

    // Session ids whose parentId == parentId, in registration order.
    [[nodiscard]] std::vector<std::string> childrenOf(const std::string& parentId) const;

    void remove(const std::string& sessionId);
    [[nodiscard]] std::size_t size() const { return order_.size(); }

private:
    struct Entry {
        SessionInfo info;
        std::unique_ptr<SessionController> controller;
    };
    std::unordered_map<std::string, Entry> entries_;
    std::vector<std::string> order_; // preserves registration order
};

} // namespace maestro::acp
