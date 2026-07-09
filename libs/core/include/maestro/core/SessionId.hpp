#pragma once
#include <string>
#include <utility>

namespace maestro::core {

// Opaque, provider-defined session identifier. CLI tools resume prior
// conversations by string handle (Claude uses a UUID); this wraps that string
// with a little type safety. An empty value means "no session".
class SessionId {
public:
    SessionId() = default;
    explicit SessionId(std::string value) : value_(std::move(value)) {}

    [[nodiscard]] const std::string& value() const noexcept { return value_; }
    [[nodiscard]] bool valid() const noexcept { return !value_.empty(); }

    friend bool operator==(const SessionId&, const SessionId&) = default;

private:
    std::string value_;
};

} // namespace maestro::core
