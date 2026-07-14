#include <chrono>
#include <future>
#include <thread>

#include "doctest.h"
#include "poll/sse_hub.hpp"

using namespace wiresprite;
using namespace std::chrono_literals;

TEST_CASE("SseHub::waitForChange returns immediately if the generation already moved past lastSeen") {
    SseHub hub;
    hub.notify();
    hub.notify();

    auto result = hub.waitForChange(0, 1s);
    REQUIRE(result.has_value());
    CHECK(*result == 2);
}

TEST_CASE("SseHub::waitForChange times out with nullopt when nothing changes") {
    SseHub hub;
    auto start = std::chrono::steady_clock::now();
    auto result = hub.waitForChange(hub.currentGeneration(), 50ms);
    auto elapsed = std::chrono::steady_clock::now() - start;

    CHECK_FALSE(result.has_value());
    CHECK_FALSE(hub.isShuttingDown());
    CHECK(elapsed >= 40ms); // roughly honored the timeout, allowing scheduler slack
}

TEST_CASE("SseHub::notify wakes a blocked waiter before its timeout") {
    SseHub hub;
    uint64_t lastSeen = hub.currentGeneration();

    auto future = std::async(std::launch::async, [&] { return hub.waitForChange(lastSeen, 5s); });

    // Give the waiter a moment to actually start blocking before notifying.
    std::this_thread::sleep_for(50ms);
    hub.notify();

    auto result = future.get();
    REQUIRE(result.has_value());
    CHECK(*result == lastSeen + 1);
}

TEST_CASE("SseHub::shutdown wakes waiters and makes subsequent waits return immediately") {
    SseHub hub;
    uint64_t lastSeen = hub.currentGeneration();

    auto future = std::async(std::launch::async, [&] { return hub.waitForChange(lastSeen, 5s); });
    std::this_thread::sleep_for(50ms);
    hub.shutdown();

    auto result = future.get();
    CHECK_FALSE(result.has_value());
    CHECK(hub.isShuttingDown());

    // A fresh wait after shutdown must not block at all.
    auto start = std::chrono::steady_clock::now();
    auto secondResult = hub.waitForChange(lastSeen, 5s);
    auto elapsed = std::chrono::steady_clock::now() - start;

    CHECK_FALSE(secondResult.has_value());
    CHECK(elapsed < 1s);
}
