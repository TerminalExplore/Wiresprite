#include "config/config.hpp"
#include "doctest.h"

using namespace wiresprite;

TEST_CASE("parseConfig parses a full, valid config") {
    const std::string ini = R"(
[http]
listen_address = 127.0.0.1
listen_port = 9090

[auth]
username = admin
password_hash = deadbeef
session_ttl_minutes = 120

[polling]
interval_seconds = 15
timeout_ms = 2000
retries = 3
max_concurrent_devices = 4
history_points = 120

# a comment line, and a blank line below

[device:procurve-2512]
display_name = HP ProCurve 2512
host = 192.168.1.1
port = 161
community = public
version = 1

[device:core-switch]
host = 10.0.0.2
version = 2c
)";

    AppConfig config = parseConfig(ini);

    CHECK(config.http.listenAddress == "127.0.0.1");
    CHECK(config.http.listenPort == 9090);

    CHECK(config.auth.username == "admin");
    CHECK(config.auth.passwordHash == "deadbeef");
    CHECK(config.auth.sessionTtlMinutes == 120);

    CHECK(config.polling.intervalSeconds == 15);
    CHECK(config.polling.timeoutMs == 2000);
    CHECK(config.polling.retries == 3);
    CHECK(config.polling.maxConcurrentDevices == 4);
    CHECK(config.polling.historyPoints == 120);

    REQUIRE(config.devices.size() == 2);

    const auto& d0 = config.devices[0];
    CHECK(d0.id == "procurve-2512");
    CHECK(d0.displayName == "HP ProCurve 2512");
    CHECK(d0.host == "192.168.1.1");
    CHECK(d0.port == 161);
    CHECK(d0.community == "public");
    CHECK(d0.version == SnmpVersion::V1);

    const auto& d1 = config.devices[1];
    CHECK(d1.id == "core-switch");
    CHECK(d1.displayName == "core-switch"); // defaults to id when display_name is omitted
    CHECK(d1.host == "10.0.0.2");
    CHECK(d1.port == 161);          // default
    CHECK(d1.community == "public"); // default
    CHECK(d1.version == SnmpVersion::V2c);
}

TEST_CASE("parseConfig applies defaults when optional sections are absent") {
    AppConfig config = parseConfig("[device:only]\nhost = 10.0.0.1\n");
    CHECK(config.http.listenPort == 8080);
    CHECK(config.auth.username == "admin");
    CHECK(config.polling.intervalSeconds == 30);
    CHECK(config.polling.historyPoints == 240);
    REQUIRE(config.devices.size() == 1);
    CHECK(config.devices[0].version == SnmpVersion::V2c);
}

TEST_CASE("parseConfig rejects malformed input with the offending line number") {
    CHECK_THROWS_WITH_AS(parseConfig("[bogus]\nkey = value\n"), doctest::Contains("line 1"), ConfigError);
    CHECK_THROWS_WITH_AS(parseConfig("[http]\nnot_a_key_value_pair\n"), doctest::Contains("line 2"), ConfigError);
    CHECK_THROWS_WITH_AS(parseConfig("[http]\nlisten_port = notanumber\n"), doctest::Contains("line 2"), ConfigError);
    CHECK_THROWS_WITH_AS(parseConfig("key = value\n"), doctest::Contains("line 1"), ConfigError);
    CHECK_THROWS_AS(parseConfig("[device:x]\nversion = 3\nhost = 1.2.3.4\n"), ConfigError);
    CHECK_THROWS_AS(parseConfig("[http]\nunknown_key = 1\n"), ConfigError);
    CHECK_THROWS_AS(parseConfig("[device:]\nhost = 1.2.3.4\n"), ConfigError);
}

TEST_CASE("parseConfig requires every device to have a host") {
    CHECK_THROWS_AS(parseConfig("[device:no-host]\ncommunity = public\n"), ConfigError);
}

TEST_CASE("parseConfig parses open_browser_on_first_run and remember_me_days") {
    AppConfig config = parseConfig(
        "[http]\nopen_browser_on_first_run = false\n[auth]\nremember_me_days = 7\n[device:x]\nhost = 1.2.3.4\n");
    CHECK(config.http.openBrowserOnFirstRun == false);
    CHECK(config.auth.rememberMeDays == 7);

    AppConfig defaults = parseConfig("[device:x]\nhost = 1.2.3.4\n");
    CHECK(defaults.http.openBrowserOnFirstRun == true);
    CHECK(defaults.auth.rememberMeDays == 30);
}

TEST_CASE("writeConfig round-trips through parseConfig") {
    AppConfig original;
    original.http.listenAddress = "127.0.0.1";
    original.http.listenPort = 9191;
    original.http.openBrowserOnFirstRun = false;
    original.auth.username = "alice";
    original.auth.passwordHash = "deadbeef";
    original.auth.sessionTtlMinutes = 45;
    original.auth.rememberMeDays = 14;
    original.polling.intervalSeconds = 5;
    original.polling.timeoutMs = 900;
    original.polling.retries = 1;
    original.polling.maxConcurrentDevices = 2;
    original.polling.historyPoints = 60;

    DeviceConfig d1;
    d1.id = "sw1";
    d1.displayName = "Switch One";
    d1.host = "10.0.0.1";
    d1.port = 161;
    d1.community = "public";
    d1.version = SnmpVersion::V1;
    original.devices.push_back(d1);

    DeviceConfig d2;
    d2.id = "sw2";
    d2.displayName = "sw2";
    d2.host = "10.0.0.2";
    d2.port = 1161;
    d2.community = "private";
    d2.version = SnmpVersion::V2c;
    original.devices.push_back(d2);

    AppConfig roundTripped = parseConfig(writeConfig(original));

    CHECK(roundTripped.http.listenAddress == original.http.listenAddress);
    CHECK(roundTripped.http.listenPort == original.http.listenPort);
    CHECK(roundTripped.http.openBrowserOnFirstRun == original.http.openBrowserOnFirstRun);
    CHECK(roundTripped.auth.username == original.auth.username);
    CHECK(roundTripped.auth.passwordHash == original.auth.passwordHash);
    CHECK(roundTripped.auth.sessionTtlMinutes == original.auth.sessionTtlMinutes);
    CHECK(roundTripped.auth.rememberMeDays == original.auth.rememberMeDays);
    CHECK(roundTripped.polling.intervalSeconds == original.polling.intervalSeconds);
    CHECK(roundTripped.polling.historyPoints == original.polling.historyPoints);

    REQUIRE(roundTripped.devices.size() == 2);
    CHECK(roundTripped.devices[0].id == "sw1");
    CHECK(roundTripped.devices[0].displayName == "Switch One");
    CHECK(roundTripped.devices[0].host == "10.0.0.1");
    CHECK(roundTripped.devices[0].version == SnmpVersion::V1);
    CHECK(roundTripped.devices[1].id == "sw2");
    CHECK(roundTripped.devices[1].port == 1161);
    CHECK(roundTripped.devices[1].community == "private");
    CHECK(roundTripped.devices[1].version == SnmpVersion::V2c);
}

TEST_CASE("writeConfig on a default-constructed AppConfig round-trips (the first-run case)") {
    AppConfig defaults;
    AppConfig roundTripped = parseConfig(writeConfig(defaults));
    CHECK(roundTripped.devices.empty());
    CHECK(roundTripped.auth.passwordHash.empty());
    CHECK(roundTripped.http.listenPort == defaults.http.listenPort);
}
