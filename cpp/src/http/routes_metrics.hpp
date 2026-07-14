#pragma once

#include <string>
#include <vector>

#include "config/config.hpp"
#include "poll/device_state.hpp"

namespace snmpmon {

// Builds the /metrics body in Prometheus text exposition format
// (https://prometheus.io/docs/instrumenting/exposition_formats/).
// Pure function, no I/O — unit-testable without a running server.
//
// Historical data and graphing are intentionally not this project's
// job: an externally run Prometheus scrapes this endpoint and Grafana
// reads from Prometheus. Left unauthenticated when mounted in
// HttpServer, matching standard Prometheus exporter convention (Phase
// 7's auth only guards the dashboard).
std::string buildMetricsText(const std::vector<DeviceConfig>& devices, const DeviceStateStore& store);

} // namespace snmpmon
