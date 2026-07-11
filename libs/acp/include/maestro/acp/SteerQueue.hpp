#pragma once
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace maestro::acp {

// A per-session FIFO of steer messages waiting to be delivered at the next turn
// boundary. This is the mechanism behind non-interrupting steering: guidance
// typed while a worker is mid-turn accumulates here and is flushed as a fresh
// turn when the current one ends, rather than interrupting mid-run.
class SteerQueue {
public:
    void enqueue(std::string message) { messages_.push_back(std::move(message)); }

    [[nodiscard]] bool empty() const { return messages_.empty(); }
    [[nodiscard]] std::size_t size() const { return messages_.size(); }

    // Return everything queued (in order) and clear the queue.
    std::vector<std::string> drain() {
        std::vector<std::string> out = std::move(messages_);
        messages_.clear();
        return out;
    }

private:
    std::vector<std::string> messages_;
};

} // namespace maestro::acp
