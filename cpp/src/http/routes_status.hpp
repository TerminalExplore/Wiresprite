#pragma once

#include <string>
#include <vector>

#include "config/config.hpp"
#include "poll/device_state.hpp"

namespace snmpmon {

// Builds the /api/status JSON body: every configured device, in
// config order, joined with its latest poll result from `store` (or a
// "not polled yet" placeholder if the poller hasn't gotten to it).
// Pure function, no I/O — unit-testable without a running server.
std::string buildStatusJson(const std::vector<DeviceConfig>& devices, const DeviceStateStore& store);

} // namespace snmpmon
