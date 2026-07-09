#pragma once
#include <cstdint>
#include <functional>

namespace maestro::core {

// Strongly-typed integer id. Prevents mixing, say, an AgentId with a TaskId.
// Tag is a phantom type; the underlying value is a 64-bit integer.
template <typename Tag>
class Id {
public:
    using value_type = std::uint64_t;

    constexpr Id() = default;
    constexpr explicit Id(value_type v) noexcept : value_(v) {}

    [[nodiscard]] constexpr value_type value() const noexcept { return value_; }
    [[nodiscard]] constexpr bool valid() const noexcept { return value_ != 0; }

    friend constexpr bool operator==(Id, Id) = default;
    friend constexpr auto operator<=>(Id, Id) = default;

private:
    value_type value_{0};
};

struct AgentTag {};
struct TaskTag {};
struct SessionTag {};
struct ProcessTag {};

using AgentId   = Id<AgentTag>;
using TaskId    = Id<TaskTag>;
using SessionId = Id<SessionTag>;
// Handle identifying a spawned process within the ProcessManager.
using ProcessHandle = Id<ProcessTag>;

} // namespace maestro::core

// Enable use of Id types as keys in unordered containers.
template <typename Tag>
struct std::hash<maestro::core::Id<Tag>> {
    std::size_t operator()(const maestro::core::Id<Tag>& id) const noexcept {
        return std::hash<std::uint64_t>{}(id.value());
    }
};
