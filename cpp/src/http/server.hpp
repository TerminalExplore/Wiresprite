#pragma once

#include <cstdint>
#include <thread>
#include <vector>

#include "config/config.hpp"
#include "httplib.h"
#include "poll/device_state.hpp"

namespace snmpmon {

// Wraps httplib::Server: registers the dashboard (/, /style.css,
// /app.js) and /api/status routes, and owns the background thread
// that serves them. /metrics (Phase 6) and auth (Phase 7) add routes
// here later; this class doesn't grow beyond route wiring + lifecycle.
class HttpServer {
public:
    HttpServer(HttpConfig config, std::vector<DeviceConfig> devices, DeviceStateStore& store);
    ~HttpServer();

    HttpServer(const HttpServer&) = delete;
    HttpServer& operator=(const HttpServer&) = delete;

    // Binds synchronously — throws std::runtime_error if the address/
    // port can't be bound — then starts accepting connections on a
    // background thread. If config.listenPort == 0, the OS picks an
    // ephemeral port; boundPort() reports which one afterward.
    void start();

    // Stops accepting connections and blocks until the background
    // thread exits. Safe to call even if start() was never called.
    void stop();

    uint16_t boundPort() const { return boundPort_; }

private:
    HttpConfig config_;
    std::vector<DeviceConfig> devices_;
    DeviceStateStore& store_;
    httplib::Server svr_;
    std::thread thread_;
    uint16_t boundPort_ = 0;
};

} // namespace snmpmon
