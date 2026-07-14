#include "poll/poller.hpp"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <mutex>
#include <sstream>

#include "poll/if_table.hpp"
#include "snmp/client.hpp"

namespace snmpmon {

namespace {

// Serializes the diagnostic log lines pollOneDevice writes from
// multiple concurrent worker threads within a cycle, so lines don't
// interleave into garbage. Not a general logging facility — just
// enough to keep Phase 4's stdout output readable; Phase 5+ can
// replace this if a real logging story is ever needed.
std::mutex& logMutex() {
    static std::mutex m;
    return m;
}

void logLine(const std::string& line) {
    std::lock_guard<std::mutex> lock(logMutex());
    std::cout << line << std::endl;
}

} // namespace

Poller::Poller(std::vector<DeviceConfig> devices, PollingConfig polling, DeviceStateStore& store)
    : devices_(std::move(devices)), polling_(polling), store_(store) {}

Poller::~Poller() {
    stop();
}

void Poller::start() {
    stopRequested_.store(false);
    thread_ = std::thread(&Poller::run, this);
}

void Poller::stop() {
    {
        std::lock_guard<std::mutex> lock(cvMutex_);
        stopRequested_.store(true);
    }
    cv_.notify_all();
    if (thread_.joinable()) {
        thread_.join();
    }
}

void Poller::run() {
    while (!stopRequested_.load()) {
        auto cycleStart = std::chrono::steady_clock::now();
        pollCycle();
        auto deadline = cycleStart + std::chrono::seconds(polling_.intervalSeconds);

        std::unique_lock<std::mutex> lock(cvMutex_);
        cv_.wait_until(lock, deadline, [this] { return stopRequested_.load(); });
    }
}

void Poller::pollCycle() {
    size_t batchSize = static_cast<size_t>(std::max(1, polling_.maxConcurrentDevices));

    for (size_t start = 0; start < devices_.size(); start += batchSize) {
        size_t end = std::min(start + batchSize, devices_.size());

        std::vector<std::thread> workers;
        workers.reserve(end - start);
        for (size_t i = start; i < end; ++i) {
            workers.emplace_back([this, i] { pollOneDevice(devices_[i]); });
        }
        for (auto& worker : workers) {
            worker.join();
        }
    }
}

void Poller::pollOneDevice(const DeviceConfig& device) {
    SnmpClient client(device.host, device.port, device.community, device.version);
    client.setTimeoutMs(polling_.timeoutMs);
    client.setRetries(polling_.retries);

    DevicePollResult result = pollIfTable(client);

    std::ostringstream line;
    line << "[poll] " << device.id << ": " << (result.reachable ? "ok" : "unreachable") << " ("
         << result.interfaces.size() << " interfaces, " << result.scrapeDurationMs << "ms)";
    logLine(line.str());

    store_.update(device.id, std::move(result));
}

} // namespace snmpmon
