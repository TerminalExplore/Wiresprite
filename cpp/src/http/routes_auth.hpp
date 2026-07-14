#pragma once

#include <optional>
#include <string>

#include "auth/auth.hpp"
#include "httplib.h"

namespace snmpmon {

// Extracts the "session" cookie's value from a request's Cookie
// header, if present. Pure/testable without a running server.
std::optional<std::string> extractSessionCookie(const httplib::Request& req);

// True if `req` carries a valid session per `auth`, or if auth is
// disabled entirely (SessionAuth::enabled() == false).
bool isAuthorized(const SessionAuth& auth, const httplib::Request& req);

// Registers GET/POST /login and POST /logout on `svr`.
void registerAuthRoutes(httplib::Server& svr, SessionAuth& auth);

} // namespace snmpmon
