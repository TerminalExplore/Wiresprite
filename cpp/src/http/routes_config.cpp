#include "http/routes_config.hpp"

#include "auth/auth.hpp"
#include "http/json_writer.hpp"

namespace wiresprite {

namespace {

int parseIntFieldOrThrow(const std::string& value, const std::string& fieldDescription) {
    try {
        size_t consumed = 0;
        int result = std::stoi(value, &consumed);
        if (consumed != value.size()) {
            throw std::invalid_argument("trailing characters");
        }
        return result;
    } catch (const std::exception&) {
        throw ConfigError(fieldDescription + " expects an integer, got \"" + value + "\"");
    }
}

uint16_t parsePortFieldOrThrow(const std::string& value, const std::string& fieldDescription) {
    int parsed = parseIntFieldOrThrow(value, fieldDescription);
    if (parsed < 1 || parsed > 65535) {
        throw ConfigError(fieldDescription + " must be a port in 1..65535, got " + value);
    }
    return static_cast<uint16_t>(parsed);
}

} // namespace

std::string buildConfigJson(const AppConfig& config) {
    std::string out = "{\"http\":{\"listenAddress\":";
    json::appendEscapedString(out, config.http.listenAddress);
    out += ",\"listenPort\":" + std::to_string(config.http.listenPort);
    out += ",\"openBrowserOnFirstRun\":";
    out += config.http.openBrowserOnFirstRun ? "true" : "false";
    out += "}";

    out += ",\"auth\":{\"username\":";
    json::appendEscapedString(out, config.auth.username);
    out += ",\"sessionTtlMinutes\":" + std::to_string(config.auth.sessionTtlMinutes);
    out += ",\"rememberMeDays\":" + std::to_string(config.auth.rememberMeDays);
    // Never echo the hash itself — the form only needs to know whether
    // one is set, so it can offer "change password" vs "set password".
    out += ",\"passwordSet\":";
    out += config.auth.passwordHash.empty() ? "false" : "true";
    out += "}";

    out += ",\"polling\":{";
    out += "\"intervalSeconds\":" + std::to_string(config.polling.intervalSeconds);
    out += ",\"timeoutMs\":" + std::to_string(config.polling.timeoutMs);
    out += ",\"retries\":" + std::to_string(config.polling.retries);
    out += ",\"maxConcurrentDevices\":" + std::to_string(config.polling.maxConcurrentDevices);
    out += ",\"historyPoints\":" + std::to_string(config.polling.historyPoints);
    out += "}";

    out += ",\"devices\":[";
    bool first = true;
    for (const auto& device : config.devices) {
        if (!first) {
            out += ",";
        }
        first = false;
        out += "{\"id\":";
        json::appendEscapedString(out, device.id);
        out += ",\"displayName\":";
        json::appendEscapedString(out, device.displayName);
        out += ",\"host\":";
        json::appendEscapedString(out, device.host);
        out += ",\"port\":" + std::to_string(device.port);
        out += ",\"community\":";
        json::appendEscapedString(out, device.community);
        out += ",\"version\":";
        json::appendEscapedString(out, device.version == SnmpVersion::V1 ? "1" : "2c");
        out += "}";
    }
    out += "]}";
    return out;
}

AppConfig parseConfigForm(const httplib::Request& req, const AppConfig& current) {
    AppConfig config;

    config.http.listenAddress = req.get_param_value("http_listen_address");
    config.http.listenPort = parsePortFieldOrThrow(req.get_param_value("http_listen_port"), "\"listen_port\"");
    config.http.openBrowserOnFirstRun = !req.get_param_value("http_open_browser").empty();

    config.auth.username = req.get_param_value("auth_username");
    // A blank "new password" field means "leave it as-is" — the form
    // never has the real hash to compare against or echo back, so this
    // is the only signal available for "unchanged".
    std::string newPassword = req.get_param_value("auth_new_password");
    config.auth.passwordHash = newPassword.empty() ? current.auth.passwordHash : sha256Hex(newPassword);
    config.auth.sessionTtlMinutes =
        parseIntFieldOrThrow(req.get_param_value("auth_session_ttl_minutes"), "\"session_ttl_minutes\"");
    config.auth.rememberMeDays =
        parseIntFieldOrThrow(req.get_param_value("auth_remember_me_days"), "\"remember_me_days\"");

    config.polling.intervalSeconds =
        parseIntFieldOrThrow(req.get_param_value("polling_interval_seconds"), "\"interval_seconds\"");
    config.polling.timeoutMs = parseIntFieldOrThrow(req.get_param_value("polling_timeout_ms"), "\"timeout_ms\"");
    config.polling.retries = parseIntFieldOrThrow(req.get_param_value("polling_retries"), "\"retries\"");
    config.polling.maxConcurrentDevices = parseIntFieldOrThrow(
        req.get_param_value("polling_max_concurrent_devices"), "\"max_concurrent_devices\"");
    config.polling.historyPoints =
        parseIntFieldOrThrow(req.get_param_value("polling_history_points"), "\"history_points\"");

    size_t deviceCount = req.get_param_value_count("device_id");
    config.devices.reserve(deviceCount);
    for (size_t i = 0; i < deviceCount; ++i) {
        DeviceConfig device;
        device.id = req.get_param_value("device_id", i);
        if (device.id.empty()) {
            throw ConfigError("device #" + std::to_string(i + 1) + " is missing an id");
        }
        device.displayName = req.get_param_value("device_display_name", i);
        if (device.displayName.empty()) {
            device.displayName = device.id;
        }
        device.host = req.get_param_value("device_host", i);

        std::string portValue = req.get_param_value("device_port", i);
        device.port = portValue.empty() ? uint16_t{161}
                                         : parsePortFieldOrThrow(portValue, "[device:" + device.id + "] \"port\"");

        device.community = req.get_param_value("device_community", i);
        if (device.community.empty()) {
            device.community = "public";
        }
        device.version = req.get_param_value("device_version", i) == "1" ? SnmpVersion::V1 : SnmpVersion::V2c;
        config.devices.push_back(std::move(device));
    }

    // Validate by round-tripping through the existing parser — reuses
    // every rule parseConfig already enforces (a device needs a host,
    // etc.) instead of re-implementing them here.
    try {
        return parseConfig(writeConfig(config));
    } catch (const ConfigError& e) {
        throw ConfigError(std::string("invalid settings: ") + e.what());
    }
}

} // namespace wiresprite
