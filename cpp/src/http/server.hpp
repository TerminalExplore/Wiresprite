#pragma once

#include <cstdint>
#include <thread>
#include <vector>

#include "auth/auth.hpp"
#include "config/config.hpp"
#include "httplib.h"
#include "poll/device_state.hpp"
#include "poll/history_store.hpp"
#include "poll/sse_hub.hpp"

namespace wiresprite {

// Wraps httplib::Server: registers the dashboard (/, /style.css,
// /app.js), /api/status, /api/events (Server-Sent Events push of the
// same JSON /api/status returns), /metrics, and the login/logout
// routes, and owns the background thread that serves them. A
// pre-routing handler guards /, /api/status, and /api/events with
// SessionAuth; /metrics, /login, and the static assets stay open (see
// server.cpp for why).
class HttpServer {
public:
    HttpServer(HttpConfig config, AuthConfig authConfig, std::vector<DeviceConfig> devices, DeviceStateStore& store,
               HistoryStore& history, SseHub& sseHub);
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
    bool authEnabled() const { return auth_.enabled(); }

private:
    HttpConfig config_;
    std::vector<DeviceConfig> devices_;
    DeviceStateStore& store_;
    HistoryStore& history_;
    SseHub& sseHub_;
    SessionAuth auth_;
    httplib::Server svr_;
    std::thread thread_;
    uint16_t boundPort_ = 0;
};

} // namespace wiresprite
