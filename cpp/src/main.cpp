#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <string>
#include <thread>

#include "config/config.hpp"
#include "http/server.hpp"
#include "poll/device_state.hpp"
#include "poll/poller.hpp"

namespace {

std::atomic<bool> gShutdownRequested{false};

void handleShutdownSignal(int) {
    // Signal-safe: only sets a flag. The actual stop()/join() calls
    // happen back on normal main-thread control flow below.
    gShutdownRequested.store(true);
}

} // namespace

// Phase 5: adds the HTTP dashboard on top of Phase 4's background
// poller. Loads the INI config, starts the Poller and HttpServer (both
// own their own background thread), and runs until Ctrl+C/SIGTERM.
// The dashboard reads DeviceStateStore on demand via /api/status;
// /metrics (Phase 6) and auth (Phase 7) build on this same server.
int main(int argc, char** argv) {
    std::string configPath = argc > 1 ? argv[1] : "snmpmon.ini";

    snmpmon::AppConfig config;
    try {
        config = snmpmon::loadConfig(configPath);
    } catch (const snmpmon::ConfigError& e) {
        std::cerr << "Failed to load config \"" << configPath << "\": " << e.what() << "\n";
        std::cerr << "See config/snmpmon.ini.example for the expected format.\n";
        return 2;
    }

    if (config.devices.empty()) {
        std::cerr << "Config \"" << configPath << "\" defines no [device:...] sections.\n";
        return 2;
    }

    std::signal(SIGINT, handleShutdownSignal);
    std::signal(SIGTERM, handleShutdownSignal);

    snmpmon::DeviceStateStore store;
    snmpmon::Poller poller(config.devices, config.polling, store);
    snmpmon::HttpServer httpServer(config.http, config.auth, config.devices, store);

    poller.start();
    try {
        httpServer.start();
    } catch (const std::exception& e) {
        std::cerr << "Failed to start HTTP server: " << e.what() << "\n";
        poller.stop();
        return 3;
    }

    std::cout << "snmpmon: polling " << config.devices.size() << " device(s) every "
              << config.polling.intervalSeconds << "s, dashboard on http://" << config.http.listenAddress << ":"
              << httpServer.boundPort() << " (Ctrl+C to stop)\n";
    if (!httpServer.authEnabled()) {
        std::cout << "snmpmon: [auth] password_hash is not set in [auth] — dashboard login is disabled.\n";
    }

    while (!gShutdownRequested.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    std::cout << "snmpmon: shutting down...\n";
    httpServer.stop();
    poller.stop();
    return 0;
}
