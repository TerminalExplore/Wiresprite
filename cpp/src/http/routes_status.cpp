#include "http/routes_status.hpp"

#include "http/json_writer.hpp"

namespace wiresprite {

namespace {

void appendHistoryJson(std::string& out, const std::vector<HistoryPoint>& points) {
    out += "[";
    bool first = true;
    for (const auto& point : points) {
        if (!first) {
            out += ",";
        }
        first = false;
        out += "{\"t\":" + std::to_string(point.unixTimeSec);
        out += ",\"inBps\":" + std::to_string(point.inBitsPerSec);
        out += ",\"outBps\":" + std::to_string(point.outBitsPerSec);
        out += "}";
    }
    out += "]";
}

void appendInterfaceJson(std::string& out, const IfEntry& iface, const std::vector<HistoryPoint>& history) {
    out += "{\"ifIndex\":" + std::to_string(iface.ifIndex);
    out += ",\"ifDescr\":";
    json::appendEscapedString(out, iface.ifDescr);
    out += ",\"ifAlias\":";
    json::appendEscapedString(out, iface.ifAlias);
    out += ",\"ifType\":" + std::to_string(iface.ifType);
    out += ",\"ifSpeed\":" + std::to_string(iface.ifSpeed);
    out += ",\"ifAdminStatus\":" + std::to_string(iface.ifAdminStatus);
    out += ",\"ifOperStatus\":" + std::to_string(iface.ifOperStatus);
    out += ",\"ifInOctets\":" + std::to_string(iface.ifInOctets);
    out += ",\"ifOutOctets\":" + std::to_string(iface.ifOutOctets);
    out += ",\"ifInErrors\":" + std::to_string(iface.ifInErrors);
    out += ",\"ifOutErrors\":" + std::to_string(iface.ifOutErrors);
    out += ",\"ifInDiscards\":" + std::to_string(iface.ifInDiscards);
    out += ",\"ifOutDiscards\":" + std::to_string(iface.ifOutDiscards);
    out += ",\"history\":";
    appendHistoryJson(out, history);
    out += "}";
}

void appendDeviceJson(std::string& out, const DeviceConfig& device, const std::optional<DevicePollResult>& result,
                       const HistoryStore& history) {
    out += "{\"id\":";
    json::appendEscapedString(out, device.id);
    out += ",\"displayName\":";
    json::appendEscapedString(out, device.displayName);
    out += ",\"host\":";
    json::appendEscapedString(out, device.host);

    bool reachable = result.has_value() && result->reachable;
    out += ",\"reachable\":";
    out += reachable ? "true" : "false";

    out += ",\"error\":";
    json::appendEscapedString(out, result.has_value() ? result->error : "not polled yet");

    out += ",\"sysUpTimeTicks\":" + std::to_string(result.has_value() ? result->sysUpTimeTicks : 0);

    out += ",\"interfaces\":[";
    if (result.has_value()) {
        bool first = true;
        for (const auto& iface : result->interfaces) {
            if (!first) {
                out += ",";
            }
            first = false;
            appendInterfaceJson(out, iface, history.get(device.id, iface.ifIndex));
        }
    }
    out += "]}";
}

} // namespace

std::string buildStatusJson(const std::vector<DeviceConfig>& devices, const DeviceStateStore& store,
                             const HistoryStore& history) {
    std::string out = "{\"devices\":[";
    bool first = true;
    for (const auto& device : devices) {
        if (!first) {
            out += ",";
        }
        first = false;
        appendDeviceJson(out, device, store.get(device.id), history);
    }
    out += "]}";
    return out;
}

} // namespace wiresprite
