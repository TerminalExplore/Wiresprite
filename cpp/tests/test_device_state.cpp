#include "doctest.h"
#include "poll/device_state.hpp"

using namespace snmpmon;

TEST_CASE("DeviceStateStore::get returns nullopt before any update") {
    DeviceStateStore store;
    CHECK_FALSE(store.get("never-polled").has_value());
}

TEST_CASE("DeviceStateStore::update then get round-trips the result") {
    DeviceStateStore store;

    DevicePollResult result;
    result.reachable = true;
    result.sysUpTimeTicks = 12345;
    result.interfaces.push_back(IfEntry{1, "eth0", 6, 1000000000, 1, 1, 100, 50, 0, 0, 0, 0});

    store.update("core-switch", result);

    auto fetched = store.get("core-switch");
    REQUIRE(fetched.has_value());
    CHECK(fetched->reachable);
    CHECK(fetched->sysUpTimeTicks == 12345);
    REQUIRE(fetched->interfaces.size() == 1);
    CHECK(fetched->interfaces[0].ifDescr == "eth0");
}

TEST_CASE("DeviceStateStore::update overwrites the previous result for the same device") {
    DeviceStateStore store;

    DevicePollResult first;
    first.reachable = false;
    first.error = "timeout";
    store.update("dev", first);

    DevicePollResult second;
    second.reachable = true;
    store.update("dev", second);

    auto fetched = store.get("dev");
    REQUIRE(fetched.has_value());
    CHECK(fetched->reachable);
    CHECK(fetched->error.empty());
}

TEST_CASE("DeviceStateStore::snapshot returns every tracked device") {
    DeviceStateStore store;
    store.update("a", DevicePollResult{});
    store.update("b", DevicePollResult{});

    auto snapshot = store.snapshot();
    CHECK(snapshot.size() == 2);
    CHECK(snapshot.count("a") == 1);
    CHECK(snapshot.count("b") == 1);
}
