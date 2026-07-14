#include "doctest.h"
#include "http/routes_status.hpp"

using namespace wiresprite;

namespace {

DeviceConfig makeDevice(std::string id, std::string displayName, std::string host) {
    DeviceConfig device;
    device.id = std::move(id);
    device.displayName = std::move(displayName);
    device.host = std::move(host);
    device.port = 161;
    device.community = "public";
    device.version = SnmpVersion::V2c;
    return device;
}

} // namespace

TEST_CASE("buildStatusJson with no devices") {
    DeviceStateStore store;
    HistoryStore history;
    CHECK(buildStatusJson({}, store, history) == "{\"devices\":[]}");
}

TEST_CASE("buildStatusJson: device not yet polled gets a placeholder, not a crash") {
    DeviceStateStore store;
    HistoryStore history;
    std::vector<DeviceConfig> devices = {makeDevice("dev1", "Device One", "10.0.0.1")};

    std::string expected =
        "{\"devices\":["
        "{\"id\":\"dev1\",\"displayName\":\"Device One\",\"host\":\"10.0.0.1\","
        "\"reachable\":false,\"error\":\"not polled yet\",\"sysUpTimeTicks\":0,\"interfaces\":[]}"
        "]}";
    CHECK(buildStatusJson(devices, store, history) == expected);
}

TEST_CASE("buildStatusJson: reachable device with interfaces, no history yet") {
    DeviceStateStore store;
    HistoryStore history;
    std::vector<DeviceConfig> devices = {makeDevice("dev1", "Device One", "10.0.0.1")};

    DevicePollResult result;
    result.reachable = true;
    result.sysUpTimeTicks = 12345;
    result.interfaces.push_back(IfEntry{1, "eth0", "", 6, 100000000, 1, 1, 1000, 500, 0, 0, 0, 0});
    store.update("dev1", result);

    std::string expected =
        "{\"devices\":["
        "{\"id\":\"dev1\",\"displayName\":\"Device One\",\"host\":\"10.0.0.1\","
        "\"reachable\":true,\"error\":\"\",\"sysUpTimeTicks\":12345,"
        "\"interfaces\":[{\"ifIndex\":1,\"ifDescr\":\"eth0\",\"ifAlias\":\"\",\"ifType\":6,\"ifSpeed\":100000000,"
        "\"ifAdminStatus\":1,\"ifOperStatus\":1,\"ifInOctets\":1000,\"ifOutOctets\":500,"
        "\"ifInErrors\":0,\"ifOutErrors\":0,\"ifInDiscards\":0,\"ifOutDiscards\":0,\"history\":[]}]}"
        "]}";
    CHECK(buildStatusJson(devices, store, history) == expected);
}

TEST_CASE("buildStatusJson: interface history reflects HistoryStore samples") {
    DeviceStateStore store;
    HistoryStore history;
    std::vector<DeviceConfig> devices = {makeDevice("dev1", "Device One", "10.0.0.1")};

    DevicePollResult first;
    first.reachable = true;
    first.polledAtUnixSec = 1000;
    first.interfaces.push_back(IfEntry{1, "eth0", "", 6, 100000000, 1, 1, 1000, 500, 0, 0, 0, 0});

    DevicePollResult second;
    second.reachable = true;
    second.polledAtUnixSec = 1010; // 10s later
    // +1000 bytes in / +500 bytes out over 10s -> 800 bps in, 400 bps out.
    second.interfaces.push_back(IfEntry{1, "eth0", "", 6, 100000000, 1, 1, 2000, 1000, 0, 0, 0, 0});

    history.record("dev1", &first, second);
    store.update("dev1", second);

    std::string json = buildStatusJson(devices, store, history);
    CHECK(json.find("\"history\":[{\"t\":1010,\"inBps\":800.000000,\"outBps\":400.000000}]") != std::string::npos);
}

TEST_CASE("buildStatusJson: unreachable device reports its error, escaped") {
    DeviceStateStore store;
    HistoryStore history;
    std::vector<DeviceConfig> devices = {makeDevice("dev1", "Device One", "10.0.0.1")};

    DevicePollResult result;
    result.reachable = false;
    result.error = "SNMP request to 10.0.0.1:161 timed out after 3 attempt(s)";
    store.update("dev1", result);

    std::string expected =
        "{\"devices\":["
        "{\"id\":\"dev1\",\"displayName\":\"Device One\",\"host\":\"10.0.0.1\","
        "\"reachable\":false,\"error\":\"SNMP request to 10.0.0.1:161 timed out after 3 attempt(s)\","
        "\"sysUpTimeTicks\":0,\"interfaces\":[]}"
        "]}";
    CHECK(buildStatusJson(devices, store, history) == expected);
}

TEST_CASE("buildStatusJson: multiple devices are joined in config order and comma-separated") {
    DeviceStateStore store;
    HistoryStore history;
    std::vector<DeviceConfig> devices = {
        makeDevice("a", "A", "10.0.0.1"),
        makeDevice("b", "B", "10.0.0.2"),
    };
    store.update("a", DevicePollResult{});

    std::string json = buildStatusJson(devices, store, history);
    size_t posA = json.find("\"id\":\"a\"");
    size_t posB = json.find("\"id\":\"b\"");
    REQUIRE(posA != std::string::npos);
    REQUIRE(posB != std::string::npos);
    CHECK(posA < posB);
    CHECK(json.find("]},{") != std::string::npos); // devices comma-joined, not concatenated
}
