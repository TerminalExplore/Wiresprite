#include "auth/auth.hpp"
#include "doctest.h"
#include "http/routes_config.hpp"

using namespace wiresprite;

namespace {

httplib::Request makeFormRequest(const httplib::Params& params) {
    httplib::Request req;
    req.params = params;
    return req;
}

} // namespace

TEST_CASE("buildConfigJson never echoes the password hash") {
    AppConfig config;
    config.auth.username = "admin";
    config.auth.passwordHash = "deadbeef";

    std::string json = buildConfigJson(config);
    CHECK(json.find("deadbeef") == std::string::npos);
    CHECK(json.find("\"passwordSet\":true") != std::string::npos);
    CHECK(json.find("\"username\":\"admin\"") != std::string::npos);
}

TEST_CASE("buildConfigJson reports passwordSet false when unconfigured") {
    AppConfig config; // default-constructed: no password
    CHECK(buildConfigJson(config).find("\"passwordSet\":false") != std::string::npos);
}

TEST_CASE("buildConfigJson lists devices with their version as a string") {
    AppConfig config;
    DeviceConfig device;
    device.id = "sw1";
    device.displayName = "Switch One";
    device.host = "10.0.0.1";
    device.port = 161;
    device.community = "public";
    device.version = SnmpVersion::V1;
    config.devices.push_back(device);

    std::string json = buildConfigJson(config);
    CHECK(json.find("\"id\":\"sw1\"") != std::string::npos);
    CHECK(json.find("\"host\":\"10.0.0.1\"") != std::string::npos);
    CHECK(json.find("\"version\":\"1\"") != std::string::npos);
}

namespace {
httplib::Params basicFormParams() {
    return httplib::Params{
        {"http_listen_address", "0.0.0.0"},
        {"http_listen_port", "8080"},
        {"auth_username", "admin"},
        {"auth_new_password", ""},
        {"auth_session_ttl_minutes", "60"},
        {"auth_remember_me_days", "30"},
        {"polling_interval_seconds", "30"},
        {"polling_timeout_ms", "1500"},
        {"polling_retries", "2"},
        {"polling_max_concurrent_devices", "8"},
        {"polling_history_points", "240"},
    };
}
} // namespace

TEST_CASE("parseConfigForm builds a config with no devices") {
    AppConfig current;
    AppConfig result = parseConfigForm(makeFormRequest(basicFormParams()), current);
    CHECK(result.devices.empty());
    CHECK(result.polling.intervalSeconds == 30);
}

TEST_CASE("parseConfigForm blank new_password keeps the current hash") {
    AppConfig current;
    current.auth.passwordHash = "existing-hash";

    AppConfig result = parseConfigForm(makeFormRequest(basicFormParams()), current);
    CHECK(result.auth.passwordHash == "existing-hash");
}

TEST_CASE("parseConfigForm a non-blank new_password gets hashed") {
    AppConfig current;
    httplib::Params params = basicFormParams();
    params.erase("auth_new_password");
    params.insert({"auth_new_password", "hunter2"});

    AppConfig result = parseConfigForm(makeFormRequest(params), current);
    CHECK(result.auth.passwordHash == sha256Hex("hunter2"));
}

TEST_CASE("parseConfigForm zips repeated device fields by position") {
    AppConfig current;
    httplib::Params params = basicFormParams();
    params.insert({"device_id", "sw1"});
    params.insert({"device_display_name", "Switch One"});
    params.insert({"device_host", "10.0.0.1"});
    params.insert({"device_port", "161"});
    params.insert({"device_community", "public"});
    params.insert({"device_version", "1"});
    params.insert({"device_id", "sw2"});
    params.insert({"device_display_name", ""}); // blank -> defaults to id
    params.insert({"device_host", "10.0.0.2"});
    params.insert({"device_port", ""}); // blank -> defaults to 161
    params.insert({"device_community", ""}); // blank -> defaults to "public"
    params.insert({"device_version", "2c"});

    AppConfig result = parseConfigForm(makeFormRequest(params), current);

    REQUIRE(result.devices.size() == 2);
    CHECK(result.devices[0].id == "sw1");
    CHECK(result.devices[0].displayName == "Switch One");
    CHECK(result.devices[0].version == SnmpVersion::V1);
    CHECK(result.devices[1].id == "sw2");
    CHECK(result.devices[1].displayName == "sw2");
    CHECK(result.devices[1].port == 161);
    CHECK(result.devices[1].community == "public");
    CHECK(result.devices[1].version == SnmpVersion::V2c);
}

TEST_CASE("parseConfigForm rejects a device with an empty id") {
    AppConfig current;
    httplib::Params params = basicFormParams();
    params.insert({"device_id", ""});
    params.insert({"device_display_name", ""});
    params.insert({"device_host", "10.0.0.1"});
    params.insert({"device_port", "161"});
    params.insert({"device_community", "public"});
    params.insert({"device_version", "2c"});

    CHECK_THROWS_AS(parseConfigForm(makeFormRequest(params), current), ConfigError);
}

TEST_CASE("parseConfigForm rejects a device with no host, via the parseConfig round-trip") {
    AppConfig current;
    httplib::Params params = basicFormParams();
    params.insert({"device_id", "sw1"});
    params.insert({"device_display_name", ""});
    params.insert({"device_host", ""});
    params.insert({"device_port", "161"});
    params.insert({"device_community", "public"});
    params.insert({"device_version", "2c"});

    CHECK_THROWS_AS(parseConfigForm(makeFormRequest(params), current), ConfigError);
}

TEST_CASE("parseConfigForm rejects a non-numeric port") {
    AppConfig current;
    httplib::Params params = basicFormParams();
    params.erase("http_listen_port");
    params.insert({"http_listen_port", "not-a-number"});

    CHECK_THROWS_AS(parseConfigForm(makeFormRequest(params), current), ConfigError);
}

TEST_CASE("parseConfigForm rejects an out-of-range port") {
    AppConfig current;
    httplib::Params params = basicFormParams();
    params.erase("http_listen_port");
    params.insert({"http_listen_port", "99999"});

    CHECK_THROWS_AS(parseConfigForm(makeFormRequest(params), current), ConfigError);
}
