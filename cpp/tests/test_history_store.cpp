#include "doctest.h"
#include "poll/history_store.hpp"

using namespace wiresprite;

TEST_CASE("computeRate: normal counter increase") {
    // +1000 bytes over 10s = 800 bits/sec.
    auto rate = computeRate(1000, 2000, 100, 110);
    REQUIRE(rate.has_value());
    CHECK(*rate == 800.0);
}

TEST_CASE("computeRate: no change over time is a valid zero rate, not skipped") {
    auto rate = computeRate(5000, 5000, 100, 110);
    REQUIRE(rate.has_value());
    CHECK(*rate == 0.0);
}

TEST_CASE("computeRate: counter decrease (reset/wraparound/reboot) is nullopt, not a negative rate") {
    CHECK_FALSE(computeRate(5000, 100, 100, 110).has_value());
}

TEST_CASE("computeRate: zero or negative elapsed time is nullopt, not a divide-by-zero") {
    CHECK_FALSE(computeRate(1000, 2000, 100, 100).has_value());  // same timestamp
    CHECK_FALSE(computeRate(1000, 2000, 110, 100).has_value());  // clock went backwards
}

namespace {

IfEntry makeIface(uint32_t ifIndex, uint64_t inOctets, uint64_t outOctets) {
    IfEntry entry;
    entry.ifIndex = ifIndex;
    entry.ifInOctets = inOctets;
    entry.ifOutOctets = outOctets;
    return entry;
}

DevicePollResult makeResult(int64_t polledAtUnixSec, std::vector<IfEntry> interfaces) {
    DevicePollResult result;
    result.reachable = true;
    result.polledAtUnixSec = polledAtUnixSec;
    result.interfaces = std::move(interfaces);
    return result;
}

} // namespace

TEST_CASE("HistoryStore::get returns empty for an unknown device or interface") {
    HistoryStore history;
    CHECK(history.get("nope", 1).empty());
}

TEST_CASE("HistoryStore::record is a no-op on the first poll (nothing to diff yet)") {
    HistoryStore history;
    DevicePollResult first = makeResult(1000, {makeIface(1, 1000, 500)});
    history.record("dev1", nullptr, first);
    CHECK(history.get("dev1", 1).empty());
}

TEST_CASE("HistoryStore::record diffs matching ifIndex and appends one point") {
    HistoryStore history;
    DevicePollResult first = makeResult(1000, {makeIface(1, 1000, 500)});
    DevicePollResult second = makeResult(1010, {makeIface(1, 2000, 1000)});

    history.record("dev1", &first, second);

    auto points = history.get("dev1", 1);
    REQUIRE(points.size() == 1);
    CHECK(points[0].unixTimeSec == 1010);
    CHECK(points[0].inBitsPerSec == 800.0);
    CHECK(points[0].outBitsPerSec == 400.0);
}

TEST_CASE("HistoryStore::record skips an interface that only appears in the new poll") {
    HistoryStore history;
    DevicePollResult first = makeResult(1000, {makeIface(1, 1000, 500)});
    DevicePollResult second = makeResult(1010, {makeIface(1, 2000, 1000), makeIface(2, 100, 100)});

    history.record("dev1", &first, second);

    CHECK(history.get("dev1", 1).size() == 1);
    CHECK(history.get("dev1", 2).empty()); // ifIndex 2 has no prior sample to diff against
}

TEST_CASE("HistoryStore::record is a no-op when either side is unreachable") {
    HistoryStore history;
    DevicePollResult reachable = makeResult(1000, {makeIface(1, 1000, 500)});
    DevicePollResult unreachable;
    unreachable.reachable = false;
    unreachable.polledAtUnixSec = 1010;

    history.record("dev1", &reachable, unreachable);
    history.record("dev1", &unreachable, reachable);

    CHECK(history.get("dev1", 1).empty());
}

TEST_CASE("HistoryStore keeps devices and interfaces isolated from each other") {
    HistoryStore history;
    DevicePollResult devAFirst = makeResult(1000, {makeIface(1, 1000, 500)});
    DevicePollResult devASecond = makeResult(1010, {makeIface(1, 2000, 1000)});
    DevicePollResult devBFirst = makeResult(1000, {makeIface(1, 9000, 9000)});
    DevicePollResult devBSecond = makeResult(1010, {makeIface(1, 9100, 9200)});

    history.record("dev-a", &devAFirst, devASecond);
    history.record("dev-b", &devBFirst, devBSecond);

    auto aPoints = history.get("dev-a", 1);
    auto bPoints = history.get("dev-b", 1);
    REQUIRE(aPoints.size() == 1);
    REQUIRE(bPoints.size() == 1);
    CHECK(aPoints[0].inBitsPerSec == 800.0);
    CHECK(bPoints[0].inBitsPerSec == 80.0); // +100 bytes over 10s = 80 bits/sec
}

TEST_CASE("HistoryStore rings out the oldest point once maxPoints is exceeded") {
    HistoryStore history(/*maxPoints=*/3);

    DevicePollResult prev = makeResult(1000, {makeIface(1, 0, 0)});
    for (int i = 1; i <= 5; ++i) {
        DevicePollResult curr = makeResult(1000 + i * 10, {makeIface(1, static_cast<uint64_t>(i) * 100, 0)});
        history.record("dev1", &prev, curr);
        prev = curr;
    }

    auto points = history.get("dev1", 1);
    REQUIRE(points.size() == 3);
    // Oldest-to-newest; the first two (from i=1,2) should have been evicted.
    CHECK(points[0].unixTimeSec == 1030);
    CHECK(points[1].unixTimeSec == 1040);
    CHECK(points[2].unixTimeSec == 1050);
}
