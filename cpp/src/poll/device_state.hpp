#pragma once

#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>

#include "poll/if_table.hpp"

namespace wiresprite {

// Thread-safe store of the latest poll result per device, keyed by
// DeviceConfig::id. The Poller's background thread writes; HTTP
// handlers (Phase 5) and the /metrics endpoint (Phase 6) read via
// get()/snapshot(), which copy out under a shared lock so readers never
// block each other and only briefly block a writer (and vice versa).
class DeviceStateStore {
public:
    void update(const std::string& deviceId, DevicePollResult result);

    // nullopt if `deviceId` hasn't completed a poll yet.
    std::optional<DevicePollResult> get(const std::string& deviceId) const;

    std::unordered_map<std::string, DevicePollResult> snapshot() const;

private:
    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, DevicePollResult> state_;
};

} // namespace wiresprite
