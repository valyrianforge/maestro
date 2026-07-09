#include "maestro/runtime/ProviderRegistry.hpp"

namespace maestro::runtime {

void ProviderRegistry::add(std::shared_ptr<IProvider> provider) {
    const std::string name = provider->id().name;
    providers_[name] = std::move(provider);
}

std::shared_ptr<IProvider> ProviderRegistry::get(const ProviderId& id) const {
    const auto it = providers_.find(id.name);
    return it == providers_.end() ? nullptr : it->second;
}

bool ProviderRegistry::contains(const ProviderId& id) const {
    return providers_.contains(id.name);
}

} // namespace maestro::runtime
