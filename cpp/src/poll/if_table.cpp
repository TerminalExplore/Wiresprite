#include "poll/if_table.hpp"

#include <map>

#include "snmp/oid.hpp"

namespace snmpmon {

namespace {

// RFC1213/RFC2863 ifEntry column numbers (1.3.6.1.2.1.2.2.1.<column>.<ifIndex>).
constexpr uint32_t kColIfDescr = 2;
constexpr uint32_t kColIfType = 3;
constexpr uint32_t kColIfSpeed = 5;
constexpr uint32_t kColIfAdminStatus = 7;
constexpr uint32_t kColIfOperStatus = 8;
constexpr uint32_t kColIfInOctets = 10;
constexpr uint32_t kColIfInDiscards = 13;
constexpr uint32_t kColIfInErrors = 14;
constexpr uint32_t kColIfOutOctets = 16;
constexpr uint32_t kColIfOutDiscards = 19;
constexpr uint32_t kColIfOutErrors = 20;

uint64_t asCounterOrZero(const ber::Value& v) {
    if (v.tag == ber::Tag::Counter32 || v.tag == ber::Tag::Gauge32 || v.tag == ber::Tag::Counter64 ||
        v.tag == ber::Tag::TimeTicks) {
        return v.asUint();
    }
    return 0;
}

int32_t asIntOrZero(const ber::Value& v) {
    return v.tag == ber::Tag::Integer ? static_cast<int32_t>(v.asInt()) : 0;
}

std::string asStringOrEmpty(const ber::Value& v) {
    return v.tag == ber::Tag::OctetString ? v.asOctetString() : std::string();
}

} // namespace

std::vector<IfEntry> bucketIfTableVarBinds(const std::vector<VarBind>& varbinds) {
    static const Oid kIfEntryBase = Oid::parse("1.3.6.1.2.1.2.2.1");
    const size_t baseSize = kIfEntryBase.size();

    std::map<uint32_t, IfEntry> byIndex; // ordered: ascending ifIndex output for free

    for (const auto& vb : varbinds) {
        const auto& comps = vb.name.components();
        if (comps.size() != baseSize + 2) {
            continue; // not a plain scalar-per-row cell; ignore defensively
        }
        uint32_t column = comps[baseSize];
        uint32_t ifIndex = comps[baseSize + 1];

        IfEntry& entry = byIndex[ifIndex];
        entry.ifIndex = ifIndex;

        switch (column) {
            case kColIfDescr:
                entry.ifDescr = asStringOrEmpty(vb.value);
                break;
            case kColIfType:
                entry.ifType = asIntOrZero(vb.value);
                break;
            case kColIfSpeed:
                entry.ifSpeed = static_cast<uint32_t>(asCounterOrZero(vb.value));
                break;
            case kColIfAdminStatus:
                entry.ifAdminStatus = asIntOrZero(vb.value);
                break;
            case kColIfOperStatus:
                entry.ifOperStatus = asIntOrZero(vb.value);
                break;
            case kColIfInOctets:
                entry.ifInOctets = asCounterOrZero(vb.value);
                break;
            case kColIfInDiscards:
                entry.ifInDiscards = asCounterOrZero(vb.value);
                break;
            case kColIfInErrors:
                entry.ifInErrors = asCounterOrZero(vb.value);
                break;
            case kColIfOutOctets:
                entry.ifOutOctets = asCounterOrZero(vb.value);
                break;
            case kColIfOutDiscards:
                entry.ifOutDiscards = asCounterOrZero(vb.value);
                break;
            case kColIfOutErrors:
                entry.ifOutErrors = asCounterOrZero(vb.value);
                break;
            default:
                break; // column we don't track (ifMtu, ifPhysAddress, ...)
        }
    }

    std::vector<IfEntry> result;
    result.reserve(byIndex.size());
    for (auto& [index, entry] : byIndex) {
        (void)index;
        result.push_back(std::move(entry));
    }
    return result;
}

DevicePollResult pollIfTable(SnmpClient& client) {
    static const Oid kIfEntryBase = Oid::parse("1.3.6.1.2.1.2.2.1");
    static const Oid kSysUpTime = Oid::parse("1.3.6.1.2.1.1.3.0");

    DevicePollResult result;
    try {
        std::vector<VarBind> varbinds = client.walkSubtree(kIfEntryBase);
        result.interfaces = bucketIfTableVarBinds(varbinds);

        SnmpGetResult uptime = client.get({kSysUpTime});
        if (uptime.errorStatus == 0 && !uptime.varBinds.empty() &&
            uptime.varBinds[0].value.tag == ber::Tag::TimeTicks) {
            result.sysUpTimeTicks = static_cast<uint32_t>(uptime.varBinds[0].value.asUint());
        }

        result.reachable = true;
    } catch (const SnmpTimeoutError& e) {
        result.reachable = false;
        result.error = e.what();
    }

    return result;
}

} // namespace snmpmon
