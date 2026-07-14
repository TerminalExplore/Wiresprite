// Integration test: a real HttpServer bound to an ephemeral loopback
// port, hit with a real httplib::Client — actual HTTP over actual
// sockets, not mocked routing.

#include "doctest.h"
#include "http/server.hpp"
#include "httplib.h"
#include "poll/device_state.hpp"

using namespace snmpmon;

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

} // namespace

TEST_CASE("HttpServer serves the dashboard, static assets, and /api/status") {
    HttpConfig httpConfig;
    httpConfig.listenAddress = "127.0.0.1";
    httpConfig.listenPort = 0; // OS-assigned, avoids clashing with a real snmpmon instance

    DeviceStateStore store;
    DevicePollResult result;
    result.reachable = true;
    result.interfaces.push_back(IfEntry{1, "eth0", 6, 1000000000, 1, 1, 10, 20, 0, 0, 0, 0});
    store.update("dev1", result);

    HttpServer server(httpConfig, {makeDevice("dev1", "10.0.0.1")}, store);
    server.start();

    httplib::Client client("127.0.0.1", server.boundPort());
    client.set_connection_timeout(2, 0);
    client.set_read_timeout(2, 0);

    SUBCASE("GET / returns the dashboard shell") {
        auto res = client.Get("/");
        REQUIRE(res != nullptr);
        CHECK(res->status == 200);
        CHECK(res->get_header_value("Content-Type").find("text/html") != std::string::npos);
        CHECK(res->body.find("<title>snmpmon</title>") != std::string::npos);
        CHECK(res->body.find("/app.js") != std::string::npos);
    }

    SUBCASE("GET /style.css returns CSS") {
        auto res = client.Get("/style.css");
        REQUIRE(res != nullptr);
        CHECK(res->status == 200);
        CHECK(res->get_header_value("Content-Type").find("text/css") != std::string::npos);
        CHECK(res->body.find(".device") != std::string::npos);
    }

    SUBCASE("GET /app.js returns JavaScript") {
        auto res = client.Get("/app.js");
        REQUIRE(res != nullptr);
        CHECK(res->status == 200);
        CHECK(res->get_header_value("Content-Type").find("javascript") != std::string::npos);
        CHECK(res->body.find("/api/status") != std::string::npos);
    }

    SUBCASE("GET /api/status returns JSON reflecting the live DeviceStateStore") {
        auto res = client.Get("/api/status");
        REQUIRE(res != nullptr);
        CHECK(res->status == 200);
        CHECK(res->get_header_value("Content-Type").find("application/json") != std::string::npos);
        CHECK(res->body == "{\"devices\":[{\"id\":\"dev1\",\"displayName\":\"dev1\",\"host\":\"10.0.0.1\","
                            "\"reachable\":true,\"error\":\"\",\"sysUpTimeTicks\":0,"
                            "\"interfaces\":[{\"ifIndex\":1,\"ifDescr\":\"eth0\",\"ifType\":6,"
                            "\"ifSpeed\":1000000000,\"ifAdminStatus\":1,\"ifOperStatus\":1,"
                            "\"ifInOctets\":10,\"ifOutOctets\":20,\"ifInErrors\":0,\"ifOutErrors\":0,"
                            "\"ifInDiscards\":0,\"ifOutDiscards\":0}]}]}");
    }

    SUBCASE("unknown route returns 404") {
        auto res = client.Get("/nope");
        REQUIRE(res != nullptr);
        CHECK(res->status == 404);
    }

    SUBCASE("/api/status reflects updates written after the server started") {
        DevicePollResult updated;
        updated.reachable = false;
        updated.error = "went unreachable";
        store.update("dev1", updated);

        auto res = client.Get("/api/status");
        REQUIRE(res != nullptr);
        CHECK(res->body.find("\"reachable\":false") != std::string::npos);
        CHECK(res->body.find("went unreachable") != std::string::npos);
    }

    server.stop();
}

TEST_CASE("HttpServer::stop is safe without start, and idempotently") {
    HttpConfig httpConfig;
    httpConfig.listenPort = 0;
    DeviceStateStore store;
    HttpServer server(httpConfig, {}, store);
    server.stop();
    server.stop();
}
