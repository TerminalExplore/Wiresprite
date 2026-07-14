// Integration test: a real HttpServer bound to an ephemeral loopback
// port, hit with a real httplib::Client — actual HTTP over actual
// sockets, not mocked routing.

#include "doctest.h"
#include "http/server.hpp"
#include "httplib.h"
#include "poll/device_state.hpp"
#include "poll/history_store.hpp"
#include "poll/sse_hub.hpp"

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

} // namespace

TEST_CASE("HttpServer serves the dashboard, static assets, and /api/status") {
    HttpConfig httpConfig;
    httpConfig.listenAddress = "127.0.0.1";
    httpConfig.listenPort = 0; // OS-assigned, avoids clashing with a real wiresprite instance

    DeviceStateStore store;
    HistoryStore history;
    SseHub sseHub;
    DevicePollResult result;
    result.reachable = true;
    result.interfaces.push_back(IfEntry{1, "eth0", "", 6, 1000000000, 1, 1, 10, 20, 0, 0, 0, 0});
    store.update("dev1", result);

    // "unused-test-config.ini" is never actually touched by this
    // TEST_CASE's subcases — only GET/POST /api/config read/write it,
    // and none of those subcases call that route.
    HttpServer server(httpConfig, AuthConfig{}, {makeDevice("dev1", "10.0.0.1")}, store, history, sseHub,
                       "unused-test-config.ini");
    server.start();

    httplib::Client client("127.0.0.1", server.boundPort());
    client.set_connection_timeout(2, 0);
    client.set_read_timeout(2, 0);

    SUBCASE("GET /api/events streams the current snapshot as an SSE frame") {
        // The connection stays open indefinitely (until the client
        // cancels or the server shuts down), so returning false from
        // the content receiver to stop after the first chunk is
        // expected here — httplib reports that as Error::Canceled and
        // the Result's Response is discarded, so the response_handler
        // (invoked once headers arrive, before any body streaming) is
        // what captures status/headers instead of the (nullptr) Result.
        std::string contentType;
        std::string received;
        client.Get(
            "/api/events",
            [&](const httplib::Response& headerRes) {
                contentType = headerRes.get_header_value("Content-Type");
                return true;
            },
            [&](const char* data, size_t len) {
                received.append(data, len);
                return false; // stop after the first chunk; this isn't a real EventSource
            });
        CHECK(contentType.find("text/event-stream") != std::string::npos);
        CHECK(received.substr(0, 6) == "data: ");
        CHECK(received.find("\"id\":\"dev1\"") != std::string::npos);
        CHECK(received.find("\"ifDescr\":\"eth0\"") != std::string::npos);
    }

    SUBCASE("GET / returns the dashboard shell") {
        auto res = client.Get("/");
        REQUIRE(res != nullptr);
        CHECK(res->status == 200);
        CHECK(res->get_header_value("Content-Type").find("text/html") != std::string::npos);
        CHECK(res->body.find("<title>Wiresprite</title>") != std::string::npos);
        CHECK(res->body.find("/app.js") != std::string::npos);
        // Logging out is meaningless with auth disabled (POST /logout ->
        // GET /login just bounces straight back to / since isAuthorized()
        // is always true) — the button shouldn't be there to click.
        CHECK(res->body.find("Log out") == std::string::npos);
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
        CHECK(res->body.find("/api/events") != std::string::npos);
    }

    SUBCASE("GET /favicon.svg returns the spiderweb mark") {
        auto res = client.Get("/favicon.svg");
        REQUIRE(res != nullptr);
        CHECK(res->status == 200);
        CHECK(res->get_header_value("Content-Type").find("image/svg+xml") != std::string::npos);
        CHECK(res->body.find("<svg") != std::string::npos);
    }

    SUBCASE("GET /api/status returns JSON reflecting the live DeviceStateStore") {
        auto res = client.Get("/api/status");
        REQUIRE(res != nullptr);
        CHECK(res->status == 200);
        CHECK(res->get_header_value("Content-Type").find("application/json") != std::string::npos);
        CHECK(res->body == "{\"devices\":[{\"id\":\"dev1\",\"displayName\":\"dev1\",\"host\":\"10.0.0.1\","
                            "\"reachable\":true,\"error\":\"\",\"sysUpTimeTicks\":0,\"sysDescr\":\"\","
                            "\"interfaces\":[{\"ifIndex\":1,\"ifDescr\":\"eth0\",\"ifAlias\":\"\",\"ifType\":6,"
                            "\"ifSpeed\":1000000000,\"ifAdminStatus\":1,\"ifOperStatus\":1,\"ifLastChange\":0,"
                            "\"ifInOctets\":10,\"ifOutOctets\":20,\"ifInErrors\":0,\"ifOutErrors\":0,"
                            "\"ifInDiscards\":0,\"ifOutDiscards\":0,\"macs\":[],\"history\":[]}]}]}");
    }

    SUBCASE("GET /metrics returns Prometheus text exposition format") {
        auto res = client.Get("/metrics");
        REQUIRE(res != nullptr);
        CHECK(res->status == 200);
        CHECK(res->get_header_value("Content-Type").find("text/plain") != std::string::npos);
        CHECK(res->body.find("# TYPE wiresprite_up gauge") != std::string::npos);
        CHECK(res->body.find("wiresprite_up{device=\"dev1\",device_ip=\"10.0.0.1\"} 1") != std::string::npos);
        CHECK(res->body.find("wiresprite_if_in_octets_total{device=\"dev1\",device_ip=\"10.0.0.1\","
                              "ifindex=\"1\",ifdescr=\"eth0\"} 10") != std::string::npos);
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
    HistoryStore history;
    SseHub sseHub;
    HttpServer server(httpConfig, AuthConfig{}, {}, store, history, sseHub, "unused-test-config.ini");
    server.stop();
    server.stop();
}

TEST_CASE("HttpServer enforces session auth when configured") {
    HttpConfig httpConfig;
    httpConfig.listenAddress = "127.0.0.1";
    httpConfig.listenPort = 0;

    AuthConfig authConfig;
    authConfig.username = "admin";
    authConfig.passwordHash = sha256Hex("hunter2");
    authConfig.sessionTtlMinutes = 60;

    DeviceStateStore store;
    HistoryStore history;
    SseHub sseHub;
    DevicePollResult result;
    result.reachable = true;
    store.update("dev1", result);

    HttpServer server(httpConfig, authConfig, {makeDevice("dev1", "10.0.0.1")}, store, history, sseHub,
                       "unused-test-config.ini");
    server.start();

    httplib::Client client("127.0.0.1", server.boundPort());
    client.set_connection_timeout(2, 0);
    client.set_read_timeout(2, 0);
    // Redirects should be inspected, not auto-followed, in these checks.
    client.set_follow_location(false);

    SUBCASE("unauthenticated access to protected paths is rejected") {
        auto dashboard = client.Get("/");
        REQUIRE(dashboard != nullptr);
        CHECK(dashboard->status == 302);
        CHECK(dashboard->get_header_value("Location") == "/login");

        auto status = client.Get("/api/status");
        REQUIRE(status != nullptr);
        CHECK(status->status == 401);
        CHECK(status->body.find("unauthorized") != std::string::npos);

        auto events = client.Get("/api/events");
        REQUIRE(events != nullptr);
        CHECK(events->status == 401);
    }

    SUBCASE("static assets and /metrics remain accessible without auth") {
        CHECK(client.Get("/style.css")->status == 200);
        CHECK(client.Get("/app.js")->status == 200);
        CHECK(client.Get("/favicon.svg")->status == 200);
        CHECK(client.Get("/metrics")->status == 200);
    }

    SUBCASE("GET /login shows the sign-in form without an error banner") {
        auto res = client.Get("/login");
        REQUIRE(res != nullptr);
        CHECK(res->status == 200);
        CHECK(res->body.find("Sign in") != std::string::npos);
        CHECK(res->body.find("Invalid username or password") == std::string::npos);
    }

    SUBCASE("POST /login with the wrong password shows an error") {
        auto res = client.Post("/login", httplib::Params{{"username", "admin"}, {"password", "not-hunter2"}});
        REQUIRE(res != nullptr);
        CHECK(res->status == 401);
        CHECK(res->body.find("Invalid username or password") != std::string::npos);
    }

    SUBCASE("POST /login with remember=on sets a persistent cookie") {
        auto res = client.Post(
            "/login", httplib::Params{{"username", "admin"}, {"password", "hunter2"}, {"remember", "on"}});
        REQUIRE(res != nullptr);
        CHECK(res->status == 302);

        std::string setCookie = res->get_header_value("Set-Cookie");
        CHECK(setCookie.find("session=") != std::string::npos);
        CHECK(setCookie.find("Max-Age=") != std::string::npos);
    }

    SUBCASE("full login -> access -> logout flow") {
        auto loginRes = client.Post("/login", httplib::Params{{"username", "admin"}, {"password", "hunter2"}});
        REQUIRE(loginRes != nullptr);
        CHECK(loginRes->status == 302);
        CHECK(loginRes->get_header_value("Location") == "/");

        std::string setCookie = loginRes->get_header_value("Set-Cookie");
        REQUIRE(setCookie.find("session=") != std::string::npos);
        REQUIRE(setCookie.find("HttpOnly") != std::string::npos);
        // Not remembered: a plain session cookie, no Max-Age, dies when
        // the browser closes regardless of the server-side session TTL.
        CHECK(setCookie.find("Max-Age") == std::string::npos);
        size_t start = setCookie.find("session=") + std::string("session=").size();
        size_t end = setCookie.find(';', start);
        std::string cookieHeader = "session=" + setCookie.substr(start, end - start);

        httplib::Headers authedHeaders = {{"Cookie", cookieHeader}};

        auto dashboard = client.Get("/", authedHeaders);
        REQUIRE(dashboard != nullptr);
        CHECK(dashboard->status == 200);
        CHECK(dashboard->body.find("Log out") != std::string::npos);

        auto status = client.Get("/api/status", authedHeaders);
        REQUIRE(status != nullptr);
        CHECK(status->status == 200);
        CHECK(status->body.find("\"reachable\":true") != std::string::npos);

        auto logoutRes = client.Post("/logout", authedHeaders);
        REQUIRE(logoutRes != nullptr);
        CHECK(logoutRes->status == 302);
        CHECK(logoutRes->get_header_value("Location") == "/login");

        // The now-destroyed session cookie no longer grants access.
        auto afterLogout = client.Get("/", authedHeaders);
        REQUIRE(afterLogout != nullptr);
        CHECK(afterLogout->status == 302);
        CHECK(afterLogout->get_header_value("Location") == "/login");
    }

    server.stop();
}
