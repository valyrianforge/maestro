#include "maestro/acp/SessionManager.hpp"

#include <algorithm>

namespace maestro::acp {

SessionController& SessionManager::add(const SessionInfo& info, AcpClient& client) {
    auto controller = std::make_unique<SessionController>(client, info.sessionId);
    SessionController& ref = *controller;
    const auto [it, inserted] = entries_.insert_or_assign(
        info.sessionId, Entry{info, std::move(controller)});
    if (inserted) {
        order_.push_back(info.sessionId);
    }
    return ref;
}

SessionController* SessionManager::controller(const std::string& sessionId) {
    const auto it = entries_.find(sessionId);
    return it == entries_.end() ? nullptr : it->second.controller.get();
}

const SessionInfo* SessionManager::info(const std::string& sessionId) const {
    const auto it = entries_.find(sessionId);
    return it == entries_.end() ? nullptr : &it->second.info;
}

std::vector<SessionInfo> SessionManager::sessions() const {
    std::vector<SessionInfo> out;
    out.reserve(order_.size());
    for (const auto& id : order_) {
        out.push_back(entries_.at(id).info);
    }
    return out;
}

std::vector<std::string> SessionManager::childrenOf(const std::string& parentId) const {
    std::vector<std::string> out;
    for (const auto& id : order_) {
        const SessionInfo& info = entries_.at(id).info;
        if (info.parentId && *info.parentId == parentId) {
            out.push_back(id);
        }
    }
    return out;
}

void SessionManager::remove(const std::string& sessionId) {
    entries_.erase(sessionId);
    order_.erase(std::remove(order_.begin(), order_.end(), sessionId), order_.end());
}

} // namespace maestro::acp
