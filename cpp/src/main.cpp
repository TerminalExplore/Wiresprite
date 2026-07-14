#include <iostream>
#include <string>

#include "config/config.hpp"
#include "poll/if_table.hpp"
#include "snmp/client.hpp"

namespace {

std::string statusLabel(int32_t status) {
    switch (status) {
        case 1:
            return "up";
        case 2:
            return "down";
        case 3:
            return "testing";
        default:
            return "unknown(" + std::to_string(status) + ")";
    }
}

void printDevice(const snmpmon::DeviceConfig& device) {
    std::cout << "== " << device.displayName << " (" << device.host << ") ==\n";
}

} // namespace

// Phase 3: config-driven multi-device poll. Loads an INI config listing
// one or more SNMP devices, walks each one's ifTable, and prints a
// summary. The background poller/HTTP dashboard that will actually
// consume this on a schedule land in Phases 4-5; this is the CLI
// checkpoint that proves walkSubtree + config parsing work together
// against more than one device.
int main(int argc, char** argv) {
    std::string configPath = argc > 1 ? argv[1] : "snmpmon.ini";

    snmpmon::AppConfig config;
    try {
        config = snmpmon::loadConfig(configPath);
    } catch (const snmpmon::ConfigError& e) {
        std::cerr << "Failed to load config \"" << configPath << "\": " << e.what() << "\n";
        std::cerr << "See config/snmpmon.ini.example for the expected format.\n";
        return 2;
    }

    if (config.devices.empty()) {
        std::cerr << "Config \"" << configPath << "\" defines no [device:...] sections.\n";
        return 2;
    }

    int unreachableCount = 0;
    for (const auto& device : config.devices) {
        printDevice(device);

        snmpmon::SnmpClient client(device.host, device.port, device.community, device.version);
        client.setTimeoutMs(config.polling.timeoutMs);
        client.setRetries(config.polling.retries);

        snmpmon::DevicePollResult result = snmpmon::pollIfTable(client);
        if (!result.reachable) {
            std::cout << "  UNREACHABLE: " << result.error << "\n\n";
            ++unreachableCount;
            continue;
        }

        std::cout << "  sysUpTime: " << result.sysUpTimeTicks << " ticks\n";
        for (const auto& iface : result.interfaces) {
            std::cout << "  [" << iface.ifIndex << "] " << iface.ifDescr << ": oper=" << statusLabel(iface.ifOperStatus)
                      << " admin=" << statusLabel(iface.ifAdminStatus) << " speed=" << iface.ifSpeed << "bps"
                      << " in=" << iface.ifInOctets << "B out=" << iface.ifOutOctets << "B"
                      << " inErr=" << iface.ifInErrors << " outErr=" << iface.ifOutErrors << "\n";
        }
        std::cout << "\n";
    }

    return unreachableCount == static_cast<int>(config.devices.size()) ? 1 : 0;
}
