#pragma once
#include <cstddef>
#include <functional>
#include <string>
#include <vector>

#include "maestro/acp/AcpClient.hpp"
#include "maestro/acp/SteerQueue.hpp"

namespace maestro::acp {

// Drives one live ACP session with turn-aware, non-interrupting steering.
//
// send() while idle starts a turn. send() while a turn is in flight queues the
// text; when the turn ends it is delivered automatically as the next turn. This
// is what makes a worker "steerable" without interrupting it mid-run. cancel()
// is the interrupting path (ACP session/cancel).
class SessionController {
public:
    enum class State { Idle, Busy };

    SessionController(AcpClient& client, std::string sessionId);

    // Instruct the agent. Delivered immediately if idle, otherwise queued and
    // flushed automatically when the current turn completes.
    void send(std::string text);

    // Interrupt the in-flight turn. Queued steer messages are retained; when the
    // cancelled turn resolves they flush as a fresh turn (redirect semantics).
    void cancel();

    // If idle with queued messages, deliver them now as one turn.
    void flushPending();

    [[nodiscard]] State state() const { return state_; }
    [[nodiscard]] const std::string& sessionId() const { return sessionId_; }
    [[nodiscard]] std::size_t pending() const { return queue_.size(); }

    // Fired at the end of each turn, before any queued steers are auto-flushed.
    std::function<void()> onTurnComplete;

private:
    void deliver(std::vector<std::string> messages);
    void handleTurnEnd();

    AcpClient& client_;
    std::string sessionId_;
    State state_ = State::Idle;
    SteerQueue queue_;
};

} // namespace maestro::acp
