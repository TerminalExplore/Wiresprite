#include <atomic>
#include <chrono>
#include <csignal>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>

#include "config/config.hpp"
#include "http/server.hpp"
#include "platform/open_browser.hpp"
#include "poll/device_state.hpp"
#include "poll/history_store.hpp"
#include "poll/poller.hpp"
#include "poll/sse_hub.hpp"

namespace {

std::atomic<bool> gShutdownRequested{false};

void handleShutdownSignal(int) {
    // Signal-safe: only sets a flag. The actual stop()/join() calls
    // happen back on normal main-thread control flow below.
    gShutdownRequested.store(true);
}

} // namespace

// Loads the INI config, starts the Poller and HttpServer (both own
// their own background thread), and runs until Ctrl+C/SIGTERM. The
// dashboard is pushed fresh DeviceStateStore/HistoryStore snapshots
// over /api/events (SSE) each time SseHub is notified at the end of a
// poll cycle, with /api/status available for one-off/non-browser
// callers; /metrics and the login flow are served off the same
// HttpServer.
int main(int argc, char** argv) {
    std::string configPath = argc > 1 ? argv[1] : "wiresprite.ini";

    // A missing config file means a genuine first run: create a default
    // one (no devices, auth disabled) rather than failing, so the
    // program comes up cleanly with an empty dashboard the user can
    // configure from /settings instead of needing to hand-write an ini
    // first. A config file that exists but fails to *parse* still fails
    // fast as before — only a missing file gets auto-created.
    bool isFirstRun = !std::filesystem::exists(configPath);

    wiresprite::AppConfig config;
    if (isFirstRun) {
        try {
            wiresprite::saveConfig(configPath, config); // config is default-constructed here
        } catch (const wiresprite::ConfigError& e) {
            std::cerr << "Failed to create config \"" << configPath << "\": " << e.what() << "\n";
            return 2;
        }
    } else {
        try {
            config = wiresprite::loadConfig(configPath);
        } catch (const wiresprite::ConfigError& e) {
            std::cerr << "Failed to load config \"" << configPath << "\": " << e.what() << "\n";
            std::cerr << "See config/wiresprite.ini.example for the expected format.\n";
            return 2;
        }
    }

    std::signal(SIGINT, handleShutdownSignal);
    std::signal(SIGTERM, handleShutdownSignal);

    wiresprite::DeviceStateStore store;
    wiresprite::HistoryStore history(static_cast<size_t>(config.polling.historyPoints));
    wiresprite::SseHub sseHub;
    wiresprite::Poller poller(config.devices, config.polling, store, history, sseHub);
    wiresprite::HttpServer httpServer(config.http, config.auth, config.devices, store, history, sseHub, configPath);

    poller.start();
    try {
        httpServer.start();
    } catch (const std::exception& e) {
        std::cerr << "Failed to start HTTP server: " << e.what() << "\n";
        poller.stop();
        return 3;
    }

    std::cout << "wiresprite: polling " << config.devices.size() << " device(s) every "
              << config.polling.intervalSeconds << "s, dashboard on http://" << config.http.listenAddress << ":"
              << httpServer.boundPort() << " (Ctrl+C to stop)\n";
    if (!httpServer.authEnabled()) {
        std::cout << "wiresprite: [auth] password_hash is not set in [auth] — dashboard login is disabled.\n";
    }

    // Only on a genuine first run — never on an ordinary restart, which
    // would be actively wrong for the systemd-managed headless
    // deployment documented in packaging/ (no display to open a browser
    // on, and popping one up unprompted on every service restart would
    // just be annoying even where there is one).
    if (isFirstRun && config.http.openBrowserOnFirstRun) {
        std::cout << "wiresprite: first run — opening the dashboard in your browser...\n";
        wiresprite::openBrowser("http://127.0.0.1:" + std::to_string(httpServer.boundPort()));
    }

    while (!gShutdownRequested.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    std::cout << "wiresprite: shutting down...\n";
    httpServer.stop();
    poller.stop();
    return 0;
}
