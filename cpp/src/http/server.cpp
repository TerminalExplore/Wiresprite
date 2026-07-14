#include "http/server.hpp"

#include <stdexcept>

#include "http/routes_metrics.hpp"
#include "http/routes_status.hpp"
#include "http/web_assets.hpp"

namespace snmpmon {

HttpServer::HttpServer(HttpConfig config, std::vector<DeviceConfig> devices, DeviceStateStore& store)
    : config_(std::move(config)), devices_(std::move(devices)), store_(store) {
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
    // Deliberately unauthenticated (Phase 7's login only guards the
    // dashboard), matching standard Prometheus exporter convention.
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
