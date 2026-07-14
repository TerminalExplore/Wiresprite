#include "config/config.hpp"

#include <cctype>
#include <fstream>
#include <sstream>

namespace wiresprite {

namespace {

std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return "";
    }
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

int parseIntOrThrow(const std::string& value, const std::string& key, int lineNumber) {
    try {
        size_t consumed = 0;
        int result = std::stoi(value, &consumed);
        if (consumed != value.size()) {
            throw std::invalid_argument("trailing characters");
        }
        return result;
    } catch (const std::exception&) {
        throw ConfigError("line " + std::to_string(lineNumber) + ": \"" + key + "\" expects an integer, got \"" +
                           value + "\"");
    }
}

uint16_t parsePortOrThrow(const std::string& value, const std::string& key, int lineNumber) {
    int parsed = parseIntOrThrow(value, key, lineNumber);
    if (parsed < 1 || parsed > 65535) {
        throw ConfigError("line " + std::to_string(lineNumber) + ": \"" + key + "\" must be a port in 1..65535, got " +
                           value);
    }
    return static_cast<uint16_t>(parsed);
}

SnmpVersion parseVersionOrThrow(const std::string& value, int lineNumber) {
    std::string lowered = value;
    for (char& c : lowered) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    if (lowered == "1") {
        return SnmpVersion::V1;
    }
    if (lowered == "2" || lowered == "2c") {
        return SnmpVersion::V2c;
    }
    throw ConfigError("line " + std::to_string(lineNumber) + ": \"version\" must be \"1\" or \"2c\", got \"" +
                       value + "\"");
}

enum class SectionKind { None, Http, Auth, Polling, Device };

} // namespace

AppConfig parseConfig(const std::string& iniText) {
    AppConfig config;
    SectionKind currentKind = SectionKind::None;
    std::string currentSectionName; // for error messages
    size_t currentDeviceIndex = 0;

    std::istringstream stream(iniText);
    std::string rawLine;
    int lineNumber = 0;

    while (std::getline(stream, rawLine)) {
        ++lineNumber;
        std::string line = trim(rawLine);

        if (line.empty() || line[0] == '#' || line[0] == ';') {
            continue;
        }

        if (line.front() == '[') {
            if (line.back() != ']') {
                throw ConfigError("line " + std::to_string(lineNumber) + ": unterminated section header \"" + line +
                                   "\"");
            }
            std::string section = trim(line.substr(1, line.size() - 2));
            currentSectionName = section;

            if (section == "http") {
                currentKind = SectionKind::Http;
            } else if (section == "auth") {
                currentKind = SectionKind::Auth;
            } else if (section == "polling") {
                currentKind = SectionKind::Polling;
            } else if (section.rfind("device:", 0) == 0) {
                std::string id = trim(section.substr(std::string("device:").size()));
                if (id.empty()) {
                    throw ConfigError("line " + std::to_string(lineNumber) + ": device section needs a name, e.g. [device:core-switch]");
                }
                DeviceConfig device;
                device.id = id;
                device.displayName = id;
                config.devices.push_back(std::move(device));
                currentDeviceIndex = config.devices.size() - 1;
                currentKind = SectionKind::Device;
            } else {
                throw ConfigError("line " + std::to_string(lineNumber) + ": unknown section \"[" + section + "]\"");
            }
            continue;
        }

        if (currentKind == SectionKind::None) {
            throw ConfigError("line " + std::to_string(lineNumber) + ": key outside of any [section]: \"" + line +
                               "\"");
        }

        size_t eq = line.find('=');
        if (eq == std::string::npos) {
            throw ConfigError("line " + std::to_string(lineNumber) + ": expected \"key = value\", got \"" + line +
                               "\"");
        }
        std::string key = trim(line.substr(0, eq));
        std::string value = trim(line.substr(eq + 1));
        if (key.empty()) {
            throw ConfigError("line " + std::to_string(lineNumber) + ": empty key");
        }

        switch (currentKind) {
            case SectionKind::Http:
                if (key == "listen_address") {
                    config.http.listenAddress = value;
                } else if (key == "listen_port") {
                    config.http.listenPort = parsePortOrThrow(value, key, lineNumber);
                } else {
                    throw ConfigError("line " + std::to_string(lineNumber) + ": unknown key \"" + key +
                                       "\" in [http]");
                }
                break;

            case SectionKind::Auth:
                if (key == "username") {
                    config.auth.username = value;
                } else if (key == "password_hash") {
                    config.auth.passwordHash = value;
                } else if (key == "session_ttl_minutes") {
                    config.auth.sessionTtlMinutes = parseIntOrThrow(value, key, lineNumber);
                } else {
                    throw ConfigError("line " + std::to_string(lineNumber) + ": unknown key \"" + key +
                                       "\" in [auth]");
                }
                break;

            case SectionKind::Polling:
                if (key == "interval_seconds") {
                    config.polling.intervalSeconds = parseIntOrThrow(value, key, lineNumber);
                } else if (key == "timeout_ms") {
                    config.polling.timeoutMs = parseIntOrThrow(value, key, lineNumber);
                } else if (key == "retries") {
                    config.polling.retries = parseIntOrThrow(value, key, lineNumber);
                } else if (key == "max_concurrent_devices") {
                    config.polling.maxConcurrentDevices = parseIntOrThrow(value, key, lineNumber);
                } else {
                    throw ConfigError("line " + std::to_string(lineNumber) + ": unknown key \"" + key +
                                       "\" in [polling]");
                }
                break;

            case SectionKind::Device: {
                DeviceConfig& device = config.devices[currentDeviceIndex];
                if (key == "display_name") {
                    device.displayName = value;
                } else if (key == "host") {
                    device.host = value;
                } else if (key == "port") {
                    device.port = parsePortOrThrow(value, key, lineNumber);
                } else if (key == "community") {
                    device.community = value;
                } else if (key == "version") {
                    device.version = parseVersionOrThrow(value, lineNumber);
                } else {
                    throw ConfigError("line " + std::to_string(lineNumber) + ": unknown key \"" + key +
                                       "\" in [device:" + device.id + "]");
                }
                break;
            }

            case SectionKind::None:
                break; // unreachable, guarded above
        }
    }

    for (const auto& device : config.devices) {
        if (device.host.empty()) {
            throw ConfigError("[device:" + device.id + "] is missing a required \"host\"");
        }
    }

    return config;
}

AppConfig loadConfig(const std::string& path) {
    std::ifstream file(path);
    if (!file) {
        throw ConfigError("cannot open config file \"" + path + "\"");
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    return parseConfig(buffer.str());
}

} // namespace wiresprite
