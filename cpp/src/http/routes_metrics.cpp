#include "http/routes_metrics.hpp"

namespace snmpmon {

namespace {

std::string escapeLabelValue(const std::string& value) {
    std::string out;
    out.reserve(value.size());
    for (char c : value) {
        switch (c) {
            case '\\':
                out += "\\\\";
                break;
            case '"':
                out += "\\\"";
                break;
            case '\n':
                out += "\\n";
                break;
            default:
                out.push_back(c);
        }
    }
    return out;
}

std::string deviceLabels(const DeviceConfig& device) {
    return "device=\"" + escapeLabelValue(device.id) + "\",device_ip=\"" + escapeLabelValue(device.host) + "\"";
}

std::string ifLabels(const DeviceConfig& device, const IfEntry& iface) {
    return deviceLabels(device) + ",ifindex=\"" + std::to_string(iface.ifIndex) + "\",ifdescr=\"" +
           escapeLabelValue(iface.ifDescr) + "\"";
}

// Accumulates one metric family's samples so its HELP/TYPE headers can
// be written once, ahead of all of that family's sample lines, per the
// Prometheus text exposition format.
class MetricFamily {
public:
    MetricFamily(std::string name, std::string help, std::string type)
        : name_(std::move(name)), help_(std::move(help)), type_(std::move(type)) {}

    void add(const std::string& labels, const std::string& value) { samples_ += name_ + "{" + labels + "} " + value + "\n"; }

    std::string str() const { return "# HELP " + name_ + " " + help_ + "\n# TYPE " + name_ + " " + type_ + "\n" + samples_; }

private:
    std::string name_;
    std::string help_;
    std::string type_;
    std::string samples_;
};

} // namespace

std::string buildMetricsText(const std::vector<DeviceConfig>& devices, const DeviceStateStore& store) {
    MetricFamily up("snmpmon_up", "Whether the last poll of this device succeeded (1) or not (0).", "gauge");
    MetricFamily scrapeDuration("snmpmon_scrape_duration_seconds", "How long the last poll of this device took.",
                                 "gauge");
    MetricFamily uptime("snmpmon_device_uptime_seconds", "Device sysUpTime, in seconds.", "gauge");
    MetricFamily ifSpeed("snmpmon_if_speed_bps", "Interface speed, in bits per second (ifSpeed).", "gauge");
    MetricFamily ifAdminStatus("snmpmon_if_admin_status",
                                "Interface admin status per RFC1213 (1=up, 2=down, 3=testing).", "gauge");
    MetricFamily ifOperStatus("snmpmon_if_oper_status",
                               "Interface operational status per RFC1213 (1=up, 2=down, 3=testing, ...).", "gauge");
    MetricFamily ifInOctets("snmpmon_if_in_octets_total", "Total octets received on the interface (ifInOctets).",
                             "counter");
    MetricFamily ifOutOctets("snmpmon_if_out_octets_total", "Total octets sent on the interface (ifOutOctets).",
                              "counter");
    MetricFamily ifInErrors("snmpmon_if_in_errors_total", "Total inbound packet errors (ifInErrors).", "counter");
    MetricFamily ifOutErrors("snmpmon_if_out_errors_total", "Total outbound packet errors (ifOutErrors).",
                              "counter");
    MetricFamily ifInDiscards("snmpmon_if_in_discards_total", "Total inbound packets discarded (ifInDiscards).",
                               "counter");
    MetricFamily ifOutDiscards("snmpmon_if_out_discards_total", "Total outbound packets discarded (ifOutDiscards).",
                                "counter");

    for (const auto& device : devices) {
        auto resultOpt = store.get(device.id);
        std::string labels = deviceLabels(device);
        bool reachable = resultOpt.has_value() && resultOpt->reachable;

        up.add(labels, reachable ? "1" : "0");

        if (!resultOpt.has_value()) {
            continue; // never polled yet: nothing else is meaningful
        }

        scrapeDuration.add(labels, std::to_string(resultOpt->scrapeDurationMs / 1000.0));

        if (!reachable) {
            continue; // no uptime/interface data for an unreachable device
        }

        uptime.add(labels, std::to_string(resultOpt->sysUpTimeTicks / 100.0));

        for (const auto& iface : resultOpt->interfaces) {
            std::string ifLbl = ifLabels(device, iface);
            ifSpeed.add(ifLbl, std::to_string(iface.ifSpeed));
            ifAdminStatus.add(ifLbl, std::to_string(iface.ifAdminStatus));
            ifOperStatus.add(ifLbl, std::to_string(iface.ifOperStatus));
            ifInOctets.add(ifLbl, std::to_string(iface.ifInOctets));
            ifOutOctets.add(ifLbl, std::to_string(iface.ifOutOctets));
            ifInErrors.add(ifLbl, std::to_string(iface.ifInErrors));
            ifOutErrors.add(ifLbl, std::to_string(iface.ifOutErrors));
            ifInDiscards.add(ifLbl, std::to_string(iface.ifInDiscards));
            ifOutDiscards.add(ifLbl, std::to_string(iface.ifOutDiscards));
        }
    }

    return up.str() + scrapeDuration.str() + uptime.str() + ifSpeed.str() + ifAdminStatus.str() + ifOperStatus.str() +
           ifInOctets.str() + ifOutOctets.str() + ifInErrors.str() + ifOutErrors.str() + ifInDiscards.str() +
           ifOutDiscards.str();
}

} // namespace snmpmon
