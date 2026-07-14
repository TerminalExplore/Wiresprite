#include "http/routes_auth.hpp"

#include "http/web_assets.hpp"

namespace snmpmon {

std::optional<std::string> extractSessionCookie(const httplib::Request& req) {
    std::string cookieHeader = req.get_header_value("Cookie");
    const std::string key = "session=";

    size_t pos = 0;
    while (pos < cookieHeader.size()) {
        size_t semi = cookieHeader.find(';', pos);
        std::string part = cookieHeader.substr(pos, semi == std::string::npos ? std::string::npos : semi - pos);

        size_t start = part.find_first_not_of(' ');
        if (start != std::string::npos) {
            part = part.substr(start);
        } else {
            part.clear();
        }

        if (part.rfind(key, 0) == 0) {
            return part.substr(key.size());
        }

        if (semi == std::string::npos) {
            break;
        }
        pos = semi + 1;
    }
    return std::nullopt;
}

bool isAuthorized(const SessionAuth& auth, const httplib::Request& req) {
    if (!auth.enabled()) {
        return true;
    }
    auto token = extractSessionCookie(req);
    return token.has_value() && auth.isValidSession(*token);
}

namespace {
// No Secure flag: this project never terminates TLS itself. HttpOnly
// keeps the token out of reach of any script (including the
// dashboard's own app.js). SameSite=Lax is enough for a same-origin
// login form and blocks the cookie being sent on cross-site requests.
constexpr const char* kCookieAttrs = "; HttpOnly; SameSite=Lax; Path=/";
} // namespace

void registerAuthRoutes(httplib::Server& svr, SessionAuth& auth) {
    svr.Get("/login", [&auth](const httplib::Request& req, httplib::Response& res) {
        if (isAuthorized(auth, req)) {
            res.set_redirect("/");
            return;
        }
        res.set_content(web::renderLoginPage(false), "text/html; charset=utf-8");
    });

    svr.Post("/login", [&auth](const httplib::Request& req, httplib::Response& res) {
        std::string username = req.get_param_value("username");
        std::string password = req.get_param_value("password");

        if (!auth.checkCredentials(username, password)) {
            res.status = 401;
            res.set_content(web::renderLoginPage(true), "text/html; charset=utf-8");
            return;
        }

        std::string token = auth.createSession();
        res.set_header("Set-Cookie", std::string("session=") + token + kCookieAttrs);
        res.set_redirect("/");
    });

    svr.Post("/logout", [&auth](const httplib::Request& req, httplib::Response& res) {
        auto token = extractSessionCookie(req);
        if (token.has_value()) {
            auth.destroySession(*token);
        }
        res.set_header("Set-Cookie", std::string("session=deleted") + kCookieAttrs + "; Max-Age=0");
        res.set_redirect("/login");
    });
}

} // namespace snmpmon
