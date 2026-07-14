#include "http/server.hpp"

#include <stdexcept>

#include "http/routes_auth.hpp"
#include "http/routes_metrics.hpp"
#include "http/routes_status.hpp"
#include "http/web_assets.hpp"

namespace snmpmon {

HttpServer::HttpServer(HttpConfig config, AuthConfig authConfig, std::vector<DeviceConfig> devices,
                        DeviceStateStore& store)
    : config_(std::move(config)),
      devices_(std::move(devices)),
      store_(store),
      auth_(authConfig.username, authConfig.passwordHash, authConfig.sessionTtlMinutes) {
    // / and /api/status carry SNMP data, so they're the two paths this
    // guards. /style.css and /app.js are just presentation code (no
    // data), and /metrics stays open by Prometheus exporter convention
    // (a scraper doesn't do cookie/session auth) — both are listed
    // explicitly here rather than defaulting protected paths to "not
    // /login/static/metrics", so adding a route later doesn't silently
    // become protected or unprotected by accident.
    svr_.set_pre_routing_handler([this](const httplib::Request& req, httplib::Response& res) {
        bool needsAuth = req.path == "/" || req.path == "/api/status";
        if (needsAuth && !isAuthorized(auth_, req)) {
            if (req.path == "/api/status") {
                res.status = 401;
                res.set_content("{\"error\":\"unauthorized\"}", "application/json");
            } else {
                res.set_redirect("/login");
            }
            return httplib::Server::HandlerResponse::Handled;
        }
        return httplib::Server::HandlerResponse::Unhandled;
    });

    registerAuthRoutes(svr_, auth_);

    svr_.Get("/", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(web::kIndexHtml, "text/html; charset=utf-8");
    });
    svr_.Get("/style.css", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(web::kStyleCss, "text/css; charset=utf-8");
    });
    svr_.Get("/app.js", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(web::kAppJs, "application/javascript; charset=utf-8");
    });
    svr_.Get("/api/status", [this](const httplib::Request&, httplib::Response& res) {
        res.set_content(buildStatusJson(devices_, store_), "application/json");
    });
    // Deliberately unauthenticated, matching standard Prometheus
    // exporter convention.
    svr_.Get("/metrics", [this](const httplib::Request&, httplib::Response& res) {
        res.set_content(buildMetricsText(devices_, store_), "text/plain; version=0.0.4; charset=utf-8");
    });
}

HttpServer::~HttpServer() {
    stop();
}

void HttpServer::start() {
    int boundPort;
    if (config_.listenPort == 0) {
        boundPort = svr_.bind_to_any_port(config_.listenAddress);
    } else {
        boundPort = svr_.bind_to_port(config_.listenAddress, config_.listenPort) ? config_.listenPort : -1;
    }
    if (boundPort < 0) {
        throw std::runtime_error("HttpServer: failed to bind " + config_.listenAddress + ":" +
                                  std::to_string(config_.listenPort));
    }
    boundPort_ = static_cast<uint16_t>(boundPort);

    thread_ = std::thread([this] { svr_.listen_after_bind(); });
    svr_.wait_until_ready();
}

void HttpServer::stop() {
    svr_.stop();
    if (thread_.joinable()) {
        thread_.join();
    }
}

} // namespace snmpmon
