#include "poll/history_store.hpp"

#include <mutex>

namespace wiresprite {

std::optional<double> computeRate(uint64_t prevOctets, uint64_t currOctets, int64_t prevTimeSec,
                                   int64_t currTimeSec) {
    if (currTimeSec <= prevTimeSec) {
        return std::nullopt;
    }
    if (currOctets < prevOctets) {
        return std::nullopt; // counter reset, 32-bit wraparound, or device reboot
    }
    double elapsedSec = static_cast<double>(currTimeSec - prevTimeSec);
    double deltaBytes = static_cast<double>(currOctets - prevOctets);
    return (deltaBytes * 8.0) / elapsedSec;
}

HistoryStore::HistoryStore(size_t maxPoints) : maxPoints_(maxPoints) {}

void HistoryStore::record(const std::string& deviceId, const DevicePollResult* previous,
                           const DevicePollResult& current) {
    if (previous == nullptr || !previous->reachable || !current.reachable) {
        return;
    }

    std::unique_lock lock(mutex_);
    auto& deviceHistory = byDevice_[deviceId];

    for (const auto& currIface : current.interfaces) {
        const IfEntry* prevIface = nullptr;
        for (const auto& candidate : previous->interfaces) {
            if (candidate.ifIndex == currIface.ifIndex) {
                prevIface = &candidate;
                break;
            }
        }
        if (prevIface == nullptr) {
            continue; // interface appeared since the last poll; nothing to diff yet
        }

        auto inRate = computeRate(prevIface->ifInOctets, currIface.ifInOctets, previous->polledAtUnixSec,
                                   current.polledAtUnixSec);
        auto outRate = computeRate(prevIface->ifOutOctets, currIface.ifOutOctets, previous->polledAtUnixSec,
                                    current.polledAtUnixSec);
        if (!inRate.has_value() && !outRate.has_value()) {
            continue;
        }

        HistoryPoint point;
        point.unixTimeSec = current.polledAtUnixSec;
        point.inBitsPerSec = inRate.value_or(0.0);
        point.outBitsPerSec = outRate.value_or(0.0);

        auto& points = deviceHistory[currIface.ifIndex];
        points.push_back(point);
        while (points.size() > maxPoints_) {
            points.pop_front();
        }
    }
}

std::vector<HistoryPoint> HistoryStore::get(const std::string& deviceId, uint32_t ifIndex) const {
    std::shared_lock lock(mutex_);
    auto deviceIt = byDevice_.find(deviceId);
    if (deviceIt == byDevice_.end()) {
        return {};
    }
    auto ifaceIt = deviceIt->second.find(ifIndex);
    if (ifaceIt == deviceIt->second.end()) {
        return {};
    }
    return std::vector<HistoryPoint>(ifaceIt->second.begin(), ifaceIt->second.end());
}

} // namespace wiresprite
