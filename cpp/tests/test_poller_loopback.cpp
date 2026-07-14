// Integration tests for Poller, over real UDP loopback + real threads,
// exercising exactly the two properties the phase plan called out:
// bounded concurrency (one slow device can't stall the others) and
// periodic cadence (it keeps re-polling, not just once).

#include "doctest.h"
#include "fake_snmp_agent.hpp"
#include "poll/device_state.hpp"
#include "poll/history_store.hpp"
#include "poll/poller.hpp"
#include "snmp/udp_socket.hpp"

#include <chrono>
#include <thread>

using namespace wiresprite;
using namespace wiresprite::test;

namespace {

DeviceConfig makeDevice(std::string id, uint16_t port) {
    DeviceConfig device;
    device.id = id;
    device.displayName = id;
    device.host = "127.0.0.1";
    device.port = port;
    device.community = "public";
    device.version = SnmpVersion::V2c;
    return device;
}

uint16_t grabUnusedPort() {
    UdpSocket probe;
    probe.bind(0);
    return probe.localPort();
}

Oid col(uint32_t column, uint32_t ifIndex) {
    return Oid::parse("1.3.6.1.2.1.2.2.1").withSuffix({column, ifIndex});
}

} // namespace

TEST_CASE("Poller polls devices concurrently, bounded by the slowest one") {
    FakeAgent fastAgent({});
    std::thread agentThread([&] { fastAgent.serveUntilIdle(800); });

    // Two unreachable devices simulate "slow" (each costs one full
    // client timeout). Polled sequentially, the cycle would take at
    // least 2 * timeoutMs; polled concurrently (maxConcurrentDevices
    // covers all 3), it should take roughly one timeoutMs.
    PollingConfig polling;
    polling.intervalSeconds = 60; // long: only the first cycle matters here
    polling.timeoutMs = 400;
    polling.retries = 0;
    polling.maxConcurrentDevices = 3;

    std::vector<DeviceConfig> devices = {
        makeDevice("fast", fastAgent.port()),
        makeDevice("slow-a", grabUnusedPort()),
        makeDevice("slow-b", grabUnusedPort()),
    };

    DeviceStateStore store;
    HistoryStore history;
    Poller poller(devices, polling, store, history);

    auto start = std::chrono::steady_clock::now();
    poller.start();

    bool allPolled = false;
    for (int i = 0; i < 200; ++i) {
        if (store.get("fast") && store.get("slow-a") && store.get("slow-b")) {
            allPolled = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    auto elapsedMs =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();

    poller.stop();
    agentThread.join();

    REQUIRE(allPolled);
    CHECK(store.get("fast")->reachable);
    CHECK_FALSE(store.get("slow-a")->reachable);
    CHECK_FALSE(store.get("slow-b")->reachable);

    // Sequential would be >= 2*400 = 800ms just from the two timeouts;
    // concurrent should land close to a single 400ms timeout. 700ms
    // comfortably separates the two without being tight enough to flake.
    CHECK(elapsedMs < 700);
}

TEST_CASE("Poller re-polls on the configured interval, not just once") {
    FakeAgent agent({});
    std::thread agentThread([&] { agent.serveUntilIdle(3000); });

    PollingConfig polling;
    polling.intervalSeconds = 1; // minimum granularity; test waits through ~2 cycles
    polling.timeoutMs = 500;
    polling.retries = 0;
    polling.maxConcurrentDevices = 1;

    std::vector<DeviceConfig> devices = {makeDevice("dev", agent.port())};

    DeviceStateStore store;
    HistoryStore history;
    Poller poller(devices, polling, store, history);
    poller.start();

    std::this_thread::sleep_for(std::chrono::milliseconds(2600));
    poller.stop();
    agentThread.join();

    // Each full poll (walkSubtree over an empty table + one sysUpTime
    // GET) is 2 requests. Three cycles fit in 2.6s at a 1s cadence
    // (t=0, t=1s, t=2s), so >=4 requests proves at least a second cycle
    // ran; a one-shot poller would top out at 2.
    CHECK(agent.requestsServed() >= 4);
}

TEST_CASE("Poller feeds HistoryStore a rate sample once it has two polls to diff") {
    std::vector<std::pair<Oid, ber::Value>> table = {
        {col(2, 1), ber::Value::octetString("eth0")},
        {col(10, 1), ber::Value::counter32(1000)},
        {col(16, 1), ber::Value::counter32(500)},
    };
    FakeAgent agent(std::move(table));
    std::thread agentThread([&] { agent.serveUntilIdle(3000); });

    PollingConfig polling;
    polling.intervalSeconds = 1;
    polling.timeoutMs = 500;
    polling.retries = 0;
    polling.maxConcurrentDevices = 1;

    std::vector<DeviceConfig> devices = {makeDevice("dev", agent.port())};

    DeviceStateStore store;
    HistoryStore history;
    Poller poller(devices, polling, store, history);
    poller.start();

    // Needs at least two completed polls of the same interface before
    // there's anything to diff; wait for a second sample to actually
    // land rather than a fixed sleep, to avoid flaking on a slow CI box.
    std::vector<HistoryPoint> points;
    for (int i = 0; i < 200; ++i) {
        points = history.get("dev", 1);
        if (!points.empty()) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    poller.stop();
    agentThread.join();

    REQUIRE_FALSE(points.empty());
    // The fake agent's counters never change between polls, so the rate
    // is legitimately zero — this confirms the Poller -> HistoryStore
    // wiring runs end to end, not that traffic is flowing.
    CHECK(points.back().inBitsPerSec == 0.0);
    CHECK(points.back().outBitsPerSec == 0.0);
    CHECK(points.back().unixTimeSec > 0);
}

TEST_CASE("Poller::stop is safe to call without start, and idempotently") {
    DeviceStateStore store;
    HistoryStore history;
    Poller poller({}, PollingConfig{}, store, history);
    poller.stop();
    poller.stop();
}
