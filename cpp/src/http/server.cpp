#include "http/server.hpp"

#include <chrono>
#include <cstdint>
#include <stdexcept>

#include "http/json_writer.hpp"
#include "http/routes_auth.hpp"
#include "http/routes_config.hpp"
#include "http/routes_metrics.hpp"
#include "http/routes_status.hpp"
#include "http/web_assets.hpp"

namespace {
// ConfigError messages routinely contain literal quotes (e.g. `"port"
// expects an integer, got "abc"`), so they need real JSON string
// escaping, not string concatenation, to stay valid JSON.
std::string jsonErrorBody(const std::string& message) {
    std::string out = "{\"error\":";
    wiresprite::json::appendEscapedString(out, message);
    out += "}";
    return out;
}
} // namespace

namespace wiresprite {

HttpServer::HttpServer(HttpConfig config, AuthConfig authConfig, std::vector<DeviceConfig> devices,
                        DeviceStateStore& store, HistoryStore& history, SseHub& sseHub, std::string configPath)
    : config_(std::move(config)),
      devices_(std::move(devices)),
      store_(store),
      history_(history),
      sseHub_(sseHub),
      auth_(authConfig.username, authConfig.passwordHash, authConfig.sessionTtlMinutes, authConfig.rememberMeDays),
      configPath_(std::move(configPath)) {
    // /, /settings, /api/status, /api/events, and /api/config all carry
    // SNMP data or let you change what's polled, so they're the paths
    // this guards. /style.css and /app.js are just presentation code
    // (no data), and /metrics stays open by Prometheus exporter
    // convention (a scraper doesn't do cookie/session auth) — both are
    // listed explicitly here rather than defaulting protected paths to
    // "not /login/static/metrics", so adding a route later doesn't
    // silently become protected or unprotected by accident.
    svr_.set_pre_routing_handler([this](const httplib::Request& req, httplib::Response& res) {
        bool needsAuth = req.path == "/" || req.path == "/settings" || req.path == "/api/status" ||
                          req.path == "/api/events" || req.path == "/api/config";
        if (needsAuth && !isAuthorized(auth_, req)) {
            if (req.path == "/" || req.path == "/settings") {
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
        // No password set and nothing configured to poll yet: this is a
        // fresh install, so send the user to setup instead of an empty
        // dashboard. Same condition renderSettingsPage uses to decide
        // between "Settings" and a welcoming first-run banner.
        if (!auth_.enabled() && devices_.empty()) {
            res.set_redirect("/settings");
            return;
        }
        res.set_content(web::renderIndexPage(auth_.enabled()), "text/html; charset=utf-8");
    });
    svr_.Get("/settings", [this](const httplib::Request&, httplib::Response& res) {
        bool isFirstRun = !auth_.enabled() && devices_.empty();
        res.set_content(web::renderSettingsPage(isFirstRun), "text/html; charset=utf-8");
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

    // The settings page (web_assets.cpp) fetches this to pre-fill its
    // form; reads straight from disk rather than tracking a redundant
    // in-memory copy, since the file is already the single source of
    // truth for anything that isn't live-applied (see POST below).
    svr_.Get("/api/config", [this](const httplib::Request&, httplib::Response& res) {
        try {
            res.set_content(buildConfigJson(loadConfig(configPath_)), "application/json");
        } catch (const ConfigError& e) {
            res.status = 500;
            res.set_content(jsonErrorBody(e.what()), "application/json");
        }
    });
    svr_.Post("/api/config", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            AppConfig current = loadConfig(configPath_);
            AppConfig updated = parseConfigForm(req, current);
            saveConfig(configPath_, updated);
            // Auth applies immediately — including setting the very
            // first password during first-run setup, which needs to
            // work without a restart to actually feel like "setup".
            // Everything else (devices, polling, HTTP listen settings)
            // is file-only; Poller/HttpServer's own device lists were
            // fixed at construction and aren't safely mutable at
            // runtime, so those need the restart the response reports.
            auth_.setCredentials(updated.auth.username, updated.auth.passwordHash);
            res.set_content("{\"saved\":true,\"restartRequired\":true}", "application/json");
        } catch (const ConfigError& e) {
            res.status = 400;
            res.set_content(jsonErrorBody(e.what()), "application/json");
        }
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
