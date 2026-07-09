#pragma once
#include <memory>
#include <string>
#include <unordered_map>

#include "maestro/core/Provider.hpp"

namespace maestro::runtime {

using core::IProvider;
using core::ProviderId;

// Maps a provider name to its implementation. The executor resolves the
// provider for each ExecRequest here, so tasks reference providers by name and
// stay decoupled from concrete types.
class ProviderRegistry {
public:
    void add(std::shared_ptr<IProvider> provider);

    [[nodiscard]] std::shared_ptr<IProvider> get(const ProviderId& id) const;
    [[nodiscard]] bool contains(const ProviderId& id) const;

private:
    std::unordered_map<std::string, std::shared_ptr<IProvider>> providers_;
};

} // namespace maestro::runtime
