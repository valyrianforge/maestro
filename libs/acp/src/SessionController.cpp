#include "maestro/acp/SessionController.hpp"

#include <utility>

namespace maestro::acp {

SessionController::SessionController(AcpClient& client, std::string sessionId)
    : client_(client), sessionId_(std::move(sessionId)) {}

void SessionController::send(std::string text) {
    if (state_ == State::Idle) {
        deliver({std::move(text)});
    } else {
        queue_.enqueue(std::move(text));
    }
}

void SessionController::cancel() {
    client_.cancel(sessionId_);
    // The in-flight prompt will resolve (stopReason cancelled) and drive
    // handleTurnEnd(), which flushes any queued redirect messages.
}

void SessionController::flushPending() {
    if (state_ == State::Idle && !queue_.empty()) {
        deliver(queue_.drain());
    }
}

void SessionController::deliver(std::vector<std::string> messages) {
    Json blocks = Json::array();
    for (auto& m : messages) {
        blocks.push_back(Json{{"type", "text"}, {"text", std::move(m)}});
    }
    state_ = State::Busy;
    client_.prompt(sessionId_, blocks, [this](const Json&, const Json*) { handleTurnEnd(); });
}

void SessionController::handleTurnEnd() {
    state_ = State::Idle;
    if (onTurnComplete) {
        onTurnComplete();
    }
    if (!queue_.empty()) {
        deliver(queue_.drain());
    }
}

} // namespace maestro::acp
