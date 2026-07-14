#include <iostream>
#include <string>

#include "snmp/client.hpp"
#include "snmp/oid.hpp"

namespace {

// Minimal display formatting for this CLI smoke test only; the real
// dashboard/metrics formatting lands in later phases.
std::string formatValue(const snmpmon::ber::Value& value) {
    using snmpmon::ber::Tag;
    switch (value.tag) {
        case Tag::OctetString:
            return "\"" + value.asOctetString() + "\"";
        case Tag::Integer:
            return std::to_string(value.asInt());
        case Tag::Counter32:
            return std::to_string(value.asUint()) + " (Counter32)";
        case Tag::Gauge32:
            return std::to_string(value.asUint()) + " (Gauge32)";
        case Tag::TimeTicks:
            return std::to_string(value.asUint()) + " (TimeTicks)";
        case Tag::Counter64:
            return std::to_string(value.asUint()) + " (Counter64)";
        case Tag::ObjectIdentifier:
            return value.asOid().toString();
        case Tag::Null:
            return "<null>";
        case Tag::NoSuchObject:
            return "<noSuchObject>";
        case Tag::NoSuchInstance:
            return "<noSuchInstance>";
        case Tag::EndOfMibView:
            return "<endOfMibView>";
        default:
            return "<unsupported>";
    }
}

} // namespace

// Phase 2 smoke test: reproduces app/snmp_monitor.py's exact GET
// (OID 1.3.6.1.2.1.2.2.1.10.1 == ifInOctets.1, community "public")
// against a real or configured device, over our own SNMP client.
int main(int argc, char** argv) {
    std::string host = argc > 1 ? argv[1] : "192.168.1.1";
    std::string community = argc > 2 ? argv[2] : "public";
    std::string oidStr = argc > 3 ? argv[3] : "1.3.6.1.2.1.2.2.1.10.1";

    snmpmon::Oid oid;
    try {
        oid = snmpmon::Oid::parse(oidStr);
    } catch (const std::exception& e) {
        std::cerr << "Invalid OID \"" << oidStr << "\": " << e.what() << "\n";
        return 2;
    }

    std::cout << "SNMP GET " << oidStr << " from " << host << " (community \"" << community << "\")...\n";

    snmpmon::SnmpClient client(host, 161, community, snmpmon::SnmpVersion::V2c);
    client.setTimeoutMs(1500);
    client.setRetries(2);

    try {
        snmpmon::SnmpGetResult result = client.get({oid});
        if (result.errorStatus != 0) {
            std::cout << "Agent returned errorStatus=" << result.errorStatus << " errorIndex=" << result.errorIndex
                      << "\n";
            return 1;
        }
        for (const auto& vb : result.varBinds) {
            std::cout << vb.name.toString() << " = " << formatValue(vb.value) << "\n";
        }
    } catch (const snmpmon::SnmpTimeoutError& e) {
        std::cerr << e.what() << "\n";
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "SNMP GET failed: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
