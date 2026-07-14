#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#include "snmp/pdu.hpp"

// Hand-rolled INI config: no JSON/YAML library dependency, matching the
// project's minimal-dependency goal for a config this small and flat.
namespace wiresprite {

struct HttpConfig {
    std::string listenAddress = "0.0.0.0";
    uint16_t listenPort = 8080;
};

struct AuthConfig {
    std::string username = "admin";
    std::string passwordHash; // SHA-256 hex; empty means auth is unconfigured (wired up in Phase 7)
    int sessionTtlMinutes = 60;
};

struct PollingConfig {
    int intervalSeconds = 30;
    int timeoutMs = 1500;
    int retries = 2;
    int maxConcurrentDevices = 8;
};

struct DeviceConfig {
    std::string id;          // the "name" in [device:name]
    std::string displayName; // defaults to id if not set
    std::string host;
    uint16_t port = 161;
    std::string community = "public";
    SnmpVersion version = SnmpVersion::V2c;
};

struct AppConfig {
    HttpConfig http;
    AuthConfig auth;
    PollingConfig polling;
    std::vector<DeviceConfig> devices;
};

class ConfigError : public std::runtime_error {
public:
    explicit ConfigError(const std::string& message) : std::runtime_error(message) {}
};

// Parses INI text (already read into memory) into an AppConfig. Split
// out from loadConfig so tests can exercise it without touching disk.
// Throws ConfigError, with a 1-based line number in the message, on
// malformed syntax, an unknown section, or an unknown key.
AppConfig parseConfig(const std::string& iniText);

// Reads `path` and parses it. Throws ConfigError if the file can't be
// opened, in addition to every parseConfig failure mode.
AppConfig loadConfig(const std::string& path);

} // namespace wiresprite
