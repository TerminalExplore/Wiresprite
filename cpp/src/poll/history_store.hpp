#pragma once

#include <cstdint>
#include <deque>
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "poll/if_table.hpp"

namespace wiresprite {

struct HistoryPoint {
    int64_t unixTimeSec = 0;
    double inBitsPerSec = 0.0;  // bits/sec, matching ifSpeed's unit
    double outBitsPerSec = 0.0;
};

// nullopt when no rate can be computed: the counter went backwards
// (a reset, 32-bit wraparound, or device reboot) or no time elapsed
// between samples — reported as "no sample" rather than a bogus
// multi-terabit spike or a divide-by-zero.
std::optional<double> computeRate(uint64_t prevOctets, uint64_t currOctets, int64_t prevTimeSec, int64_t currTimeSec);

// A small in-memory ring buffer of recent traffic-rate samples per
// (device, interface), purely so the dashboard has something to plot
// immediately on load instead of starting from a blank chart. This is
// not a time-series database: fixed size, in-memory only, lost on
// restart. Long-term history is Prometheus's job (see /metrics), not
// this project's — see cpp/README.md.
class HistoryStore {
public:
    explicit HistoryStore(size_t maxPoints = 60);

    // Diffs matching ifIndex entries between `previous` (the device's
    // prior poll result, or nullptr on its first ever poll) and
    // `current`, appending one HistoryPoint per interface where a rate
    // is computable. No-op when `previous` is null or either result is
    // unreachable (nothing meaningful to diff).
    void record(const std::string& deviceId, const DevicePollResult* previous, const DevicePollResult& current);

    // Oldest-to-newest. Empty if this device/interface has no samples yet.
    std::vector<HistoryPoint> get(const std::string& deviceId, uint32_t ifIndex) const;

private:
    size_t maxPoints_;
    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, std::unordered_map<uint32_t, std::deque<HistoryPoint>>> byDevice_;
};

} // namespace wiresprite
