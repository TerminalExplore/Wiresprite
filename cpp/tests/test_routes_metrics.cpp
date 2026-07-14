#include "doctest.h"
#include "http/routes_metrics.hpp"

using namespace wiresprite;

namespace {

DeviceConfig makeDevice(std::string id, std::string host) {
    DeviceConfig device;
    device.id = std::move(id);
    device.displayName = device.id;
    device.host = std::move(host);
    device.port = 161;
    device.community = "public";
    device.version = SnmpVersion::V2c;
    return device;
}

size_t countOccurrences(const std::string& haystack, const std::string& needle) {
    size_t count = 0;
    size_t pos = 0;
    while ((pos = haystack.find(needle, pos)) != std::string::npos) {
        ++count;
        pos += needle.size();
    }
    return count;
}

} // namespace

TEST_CASE("buildMetricsText with no devices emits headers but no samples") {
    DeviceStateStore store;
    std::string text = buildMetricsText({}, store);
    CHECK(text.find("# TYPE wiresprite_up gauge") != std::string::npos);
    CHECK(text.find("wiresprite_up{") == std::string::npos);
    CHECK(text.find("# TYPE wiresprite_if_in_octets_total counter") != std::string::npos);
}

TEST_CASE("buildMetricsText: never-polled device only gets wiresprite_up") {
    DeviceStateStore store;
    std::vector<DeviceConfig> devices = {makeDevice("dev1", "10.0.0.1")};

    std::string text = buildMetricsText(devices, store);
    CHECK(text.find("wiresprite_up{device=\"dev1\",device_ip=\"10.0.0.1\"} 0") != std::string::npos);
    CHECK(text.find("wiresprite_scrape_duration_seconds{device=\"dev1\"") == std::string::npos);
    CHECK(text.find("wiresprite_device_uptime_seconds{device=\"dev1\"") == std::string::npos);
}

TEST_CASE("buildMetricsText: unreachable device gets up=0 and scrape duration, nothing else") {
    DeviceStateStore store;
    std::vector<DeviceConfig> devices = {makeDevice("dev1", "10.0.0.1")};

    DevicePollResult result;
    result.reachable = false;
    result.error = "timed out";
    result.scrapeDurationMs = 1500;
    store.update("dev1", result);

    std::string text = buildMetricsText(devices, store);
    CHECK(text.find("wiresprite_up{device=\"dev1\",device_ip=\"10.0.0.1\"} 0") != std::string::npos);
    CHECK(text.find("wiresprite_scrape_duration_seconds{device=\"dev1\",device_ip=\"10.0.0.1\"} 1.500000") !=
          std::string::npos);
    CHECK(text.find("wiresprite_device_uptime_seconds{device=\"dev1\"") == std::string::npos);
    CHECK(text.find("wiresprite_if_speed_bps{") == std::string::npos);
}

TEST_CASE("buildMetricsText: reachable device with interfaces — exact sample values") {
    DeviceStateStore store;
    std::vector<DeviceConfig> devices = {makeDevice("core-switch", "10.0.0.2")};

    DevicePollResult result;
    result.reachable = true;
    result.sysUpTimeTicks = 123456; // -> 1234.56 seconds
    result.scrapeDurationMs = 42;
    result.interfaces.push_back(IfEntry{1, "eth0", "", 6, 1000000000, 1, 1, 111, 222, 3, 4, 5, 6});
    store.update("core-switch", result);

    std::string text = buildMetricsText(devices, store);

    const std::string labels = "device=\"core-switch\",device_ip=\"10.0.0.2\"";
    const std::string ifLabels = labels + ",ifindex=\"1\",ifdescr=\"eth0\"";

    CHECK(text.find("wiresprite_up{" + labels + "} 1") != std::string::npos);
    CHECK(text.find("wiresprite_scrape_duration_seconds{" + labels + "} 0.042000") != std::string::npos);
    CHECK(text.find("wiresprite_device_uptime_seconds{" + labels + "} 1234.560000") != std::string::npos);
    CHECK(text.find("wiresprite_if_speed_bps{" + ifLabels + "} 1000000000") != std::string::npos);
    CHECK(text.find("wiresprite_if_admin_status{" + ifLabels + "} 1") != std::string::npos);
    CHECK(text.find("wiresprite_if_oper_status{" + ifLabels + "} 1") != std::string::npos);
    CHECK(text.find("wiresprite_if_in_octets_total{" + ifLabels + "} 111") != std::string::npos);
    CHECK(text.find("wiresprite_if_out_octets_total{" + ifLabels + "} 222") != std::string::npos);
    CHECK(text.find("wiresprite_if_in_errors_total{" + ifLabels + "} 3") != std::string::npos);
    CHECK(text.find("wiresprite_if_out_errors_total{" + ifLabels + "} 4") != std::string::npos);
    CHECK(text.find("wiresprite_if_in_discards_total{" + ifLabels + "} 5") != std::string::npos);
    CHECK(text.find("wiresprite_if_out_discards_total{" + ifLabels + "} 6") != std::string::npos);
}

TEST_CASE("buildMetricsText escapes label values") {
    DeviceStateStore store;
    std::vector<DeviceConfig> devices = {makeDevice("dev1", "10.0.0.1")};

    DevicePollResult result;
    result.reachable = true;
    result.interfaces.push_back(IfEntry{1, "eth0 \"WAN\" \\uplink", "", 6, 0, 1, 1, 0, 0, 0, 0, 0, 0});
    store.update("dev1", result);

    std::string text = buildMetricsText(devices, store);
    CHECK(text.find("ifdescr=\"eth0 \\\"WAN\\\" \\\\uplink\"") != std::string::npos);
    // The escaped quote must not terminate the label value early.
    CHECK(text.find("wiresprite_if_speed_bps{device=\"dev1\",device_ip=\"10.0.0.1\",ifindex=\"1\","
                     "ifdescr=\"eth0 \\\"WAN\\\" \\\\uplink\"} 0") != std::string::npos);
}

TEST_CASE("buildMetricsText groups samples by metric family, not by device") {
    DeviceStateStore store;
    std::vector<DeviceConfig> devices = {makeDevice("a", "10.0.0.1"), makeDevice("b", "10.0.0.2")};
    store.update("a", DevicePollResult{});
    store.update("b", DevicePollResult{});

    std::string text = buildMetricsText(devices, store);
    // Exactly one HELP/TYPE pair per family, regardless of device count.
    CHECK(countOccurrences(text, "# TYPE wiresprite_up gauge") == 1);
    CHECK(countOccurrences(text, "wiresprite_up{") == 2);

    // Both devices' wiresprite_up samples must appear together, not
    // interleaved with a different family's samples in between.
    size_t firstUp = text.find("wiresprite_up{device=\"a\"");
    size_t secondUp = text.find("wiresprite_up{device=\"b\"");
    size_t nextFamily = text.find("# TYPE wiresprite_scrape_duration_seconds");
    REQUIRE(firstUp != std::string::npos);
    REQUIRE(secondUp != std::string::npos);
    REQUIRE(nextFamily != std::string::npos);
    CHECK(firstUp < secondUp);
    CHECK(secondUp < nextFamily);
}
