#include "poll/device_state.hpp"

#include <mutex>

namespace snmpmon {

void DeviceStateStore::update(const std::string& deviceId, DevicePollResult result) {
    std::unique_lock lock(mutex_);
    state_[deviceId] = std::move(result);
}

std::optional<DevicePollResult> DeviceStateStore::get(const std::string& deviceId) const {
    std::shared_lock lock(mutex_);
    auto it = state_.find(deviceId);
    if (it == state_.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::unordered_map<std::string, DevicePollResult> DeviceStateStore::snapshot() const {
    std::shared_lock lock(mutex_);
    return state_;
}

} // namespace snmpmon
