#include "doctest.h"
#include "http/routes_auth.hpp"

using namespace snmpmon;

namespace {
httplib::Request requestWithCookie(const std::string& cookieHeader) {
    httplib::Request req;
    if (!cookieHeader.empty()) {
        req.set_header("Cookie", cookieHeader);
    }
    return req;
}
} // namespace

TEST_CASE("extractSessionCookie: no Cookie header") {
    httplib::Request req;
    CHECK_FALSE(extractSessionCookie(req).has_value());
}

TEST_CASE("extractSessionCookie: single session cookie") {
    auto token = extractSessionCookie(requestWithCookie("session=abc123"));
    REQUIRE(token.has_value());
    CHECK(*token == "abc123");
}

TEST_CASE("extractSessionCookie: session cookie among others, either position") {
    CHECK(extractSessionCookie(requestWithCookie("foo=bar; session=abc123; baz=qux")).value() == "abc123");
    CHECK(extractSessionCookie(requestWithCookie("session=abc123; other=1")).value() == "abc123");
    CHECK(extractSessionCookie(requestWithCookie("other=1; session=abc123")).value() == "abc123");
}

TEST_CASE("extractSessionCookie: no matching cookie") {
    CHECK_FALSE(extractSessionCookie(requestWithCookie("foo=bar; baz=qux")).has_value());
}

TEST_CASE("extractSessionCookie: tolerates missing/irregular spacing") {
    CHECK(extractSessionCookie(requestWithCookie("foo=bar;session=abc123")).value() == "abc123");
    CHECK(extractSessionCookie(requestWithCookie("foo=bar;   session=abc123")).value() == "abc123");
}

TEST_CASE("isAuthorized: always true when auth is disabled") {
    SessionAuth auth("admin", "", 60);
    httplib::Request req; // no cookie at all
    CHECK(isAuthorized(auth, req));
}

TEST_CASE("isAuthorized: false without a cookie when auth is enabled") {
    SessionAuth auth("admin", sha256Hex("hunter2"), 60);
    httplib::Request req;
    CHECK_FALSE(isAuthorized(auth, req));
}

TEST_CASE("isAuthorized: true with a valid session cookie, false with a bogus one") {
    SessionAuth auth("admin", sha256Hex("hunter2"), 60);
    std::string token = auth.createSession();

    CHECK(isAuthorized(auth, requestWithCookie("session=" + token)));
    CHECK_FALSE(isAuthorized(auth, requestWithCookie("session=not-the-real-token")));
}
