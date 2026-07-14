#pragma once

#include <string>
#include <vector>

#include "config/config.hpp"
#include "poll/device_state.hpp"
#include "poll/history_store.hpp"

namespace wiresprite {

// Builds the /api/status JSON body: every configured device, in
// config order, joined with its latest poll result from `store` (or a
// "not polled yet" placeholder if the poller hasn't gotten to it) and
// each interface's recent traffic-rate samples from `history`.
// Pure function, no I/O — unit-testable without a running server.
std::string buildStatusJson(const std::vector<DeviceConfig>& devices, const DeviceStateStore& store,
                             const HistoryStore& history);

} // namespace wiresprite
