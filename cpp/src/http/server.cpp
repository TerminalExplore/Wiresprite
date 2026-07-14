#include "http/server.hpp"

#include <chrono>
#include <cstdint>
#include <stdexcept>

#include "http/routes_auth.hpp"
#include "http/routes_metrics.hpp"
#include "http/routes_status.hpp"
#include "http/web_assets.hpp"

namespace wiresprite {

HttpServer::HttpServer(HttpConfig config, AuthConfig authConfig, std::vector<DeviceConfig> devices,
                        DeviceStateStore& store, HistoryStore& history, SseHub& sseHub)
    : config_(std::move(config)),
      devices_(std::move(devices)),
      store_(store),
      history_(history),
      sseHub_(sseHub),
      auth_(authConfig.username, authConfig.passwordHash, authConfig.sessionTtlMinutes) {
    // /, /api/status, and /api/events all carry SNMP data, so they're
    // the paths this guards. /style.css and /app.js are just
    // presentation code (no data), and /metrics stays open by
    // Prometheus exporter convention (a scraper doesn't do
    // cookie/session auth) — both are listed explicitly here rather
    // than defaulting protected paths to "not /login/static/metrics",
    // so adding a route later doesn't silently become protected or
    // unprotected by accident.
    svr_.set_pre_routing_handler([this](const httplib::Request& req, httplib::Response& res) {
        bool needsAuth = req.path == "/" || req.path == "/api/status" || req.path == "/api/events";
        if (needsAuth && !isAuthorized(auth_, req)) {
            if (req.path == "/") {
                res.set_redirect("/login");
            } else {
                res.status = 401;
                res.set_content("{\"error\":\"unauthorized\"}", "application/json");
            }
            return httplib::Server::HandlerResponse::Handled;
        }
        return httplib::Server::HandlerResponse::Unhandled;
    });

    registerAuthRoutes(svr_, auth_);

    svr_.Get("/", [this](const httplib::Request&, httplib::Response& res) {
        res.set_content(web::renderIndexPage(auth_.enabled()), "text/html; charset=utf-8");
    });
    svr_.Get("/style.css", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(web::kStyleCss, "text/css; charset=utf-8");
    });
    svr_.Get("/app.js", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(web::kAppJs, "application/javascript; charset=utf-8");
    });
    svr_.Get("/favicon.svg", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(web::kFaviconSvg, "image/svg+xml");
    });
    svr_.Get("/api/status", [this](const httplib::Request&, httplib::Response& res) {
        res.set_content(buildStatusJson(devices_, store_, history_), "application/json");
    });
    // Server-Sent Events: pushes the same JSON /api/status returns
    // every time a poll cycle completes, instead of making the
    // dashboard poll on a timer. httplib calls this provider
    // repeatedly on the connection's own worker thread until it
    // returns false; `first`/`lastSeen` are captured by value in this
    // mutable lambda, so they persist across those repeated calls for
    // the lifetime of one connection.
    svr_.Get("/api/events", [this](const httplib::Request&, httplib::Response& res) {
        res.set_chunked_content_provider(
            "text/event-stream", [this, first = true, lastSeen = uint64_t{0}](
                                      size_t, httplib::DataSink& sink) mutable {
                std::string payload;
                if (first) {
                    first = false;
                    lastSeen = sseHub_.currentGeneration();
                    payload = buildStatusJson(devices_, store_, history_);
                } else {
                    auto generation = sseHub_.waitForChange(lastSeen, std::chrono::seconds(20));
                    if (!generation.has_value()) {
                        if (sseHub_.isShuttingDown()) {
                            return false; // ends the stream
                        }
                        static const std::string keepalive = ": keepalive\n\n";
                        return sink.write(keepalive.data(), keepalive.size());
                    }
                    lastSeen = *generation;
                    payload = buildStatusJson(devices_, store_, history_);
                }
                std::string frame = "data: " + payload + "\n\n";
                return sink.write(frame.data(), frame.size());
            });
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
    // Unblocks any /api/events connections immediately instead of
    // leaving them to wait out their keep-alive timeout while the
    // server is shutting down.
    sseHub_.shutdown();
    svr_.stop();
    if (thread_.joinable()) {
        thread_.join();
    }
}

} // namespace wiresprite
