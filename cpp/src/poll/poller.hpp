#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

#include "config/config.hpp"
#include "poll/device_state.hpp"
#include "poll/history_store.hpp"
#include "poll/sse_hub.hpp"

namespace wiresprite {

// Owns a background thread that polls every configured device on a
// fixed cadence (PollingConfig::intervalSeconds), writing each result
// into a DeviceStateStore as soon as it's known. Each poll cycle polls
// devices concurrently in batches bounded by
// PollingConfig::maxConcurrentDevices, so one slow or unreachable
// device delays only its own batch, not the whole cycle. Also feeds
// HistoryStore a rate sample per interface each cycle, diffing this
// poll's counters against the previous one still sitting in
// DeviceStateStore, and notifies SseHub once per cycle so /api/events
// can push the new snapshot to connected dashboards.
class Poller {
public:
    Poller(std::vector<DeviceConfig> devices, PollingConfig polling, DeviceStateStore& store, HistoryStore& history,
           SseHub& sseHub);
    ~Poller();

    Poller(const Poller&) = delete;
    Poller& operator=(const Poller&) = delete;

    // Starts the background thread. Safe to call once; call stop()
    // before start() again if restarting.
    void start();

    // Requests shutdown and blocks until the background thread exits.
    // A cycle already in flight finishes its current batch before
    // noticing the request (individual device polls aren't cancelled
    // mid-flight). Safe to call even if start() was never called.
    void stop();

private:
    void run();
    void pollCycle();
    void pollOneDevice(const DeviceConfig& device);

    std::vector<DeviceConfig> devices_;
    PollingConfig polling_;
    DeviceStateStore& store_;
    HistoryStore& history_;
    SseHub& sseHub_;

    std::thread thread_;
    std::atomic<bool> stopRequested_{false};
    std::mutex cvMutex_;
    std::condition_variable cv_; // lets stop() interrupt the between-cycle sleep immediately
};

} // namespace wiresprite
