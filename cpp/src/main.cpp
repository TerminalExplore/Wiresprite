#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <string>
#include <thread>

#include "config/config.hpp"
#include "poll/device_state.hpp"
#include "poll/poller.hpp"

namespace {

std::atomic<bool> gShutdownRequested{false};

void handleShutdownSignal(int) {
    // Signal-safe: only sets a flag. The actual poller.stop()/join()
    // happens back on the normal main-thread control flow below.
    gShutdownRequested.store(true);
}

} // namespace

// Phase 4: config-driven continuous monitor. Loads the INI config,
// starts a background Poller that walks every device's ifTable on
// PollingConfig::intervalSeconds cadence, and runs until Ctrl+C
// (SIGINT) or SIGTERM, then shuts the poller down cleanly. The HTTP
// dashboard that will actually read DeviceStateStore lands in Phase 5;
// this is the headless checkpoint proving the poller itself works.
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
    poller.start();

    std::cout << "snmpmon: polling " << config.devices.size() << " device(s) every "
              << config.polling.intervalSeconds << "s (Ctrl+C to stop)\n";

    while (!gShutdownRequested.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    std::cout << "snmpmon: shutting down...\n";
    poller.stop();
    return 0;
}
