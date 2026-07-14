#include "poll/if_table.hpp"

#include <chrono>
#include <map>

#include "snmp/oid.hpp"

namespace wiresprite {

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
constexpr uint32_t kColIfLastChange = 9;

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
            case kColIfLastChange:
                entry.ifLastChangeTicks = static_cast<uint32_t>(asCounterOrZero(vb.value));
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

void mergeIfAlias(std::vector<IfEntry>& entries, const std::vector<VarBind>& ifAliasVarbinds) {
    static const Oid kIfAlias = Oid::parse("1.3.6.1.2.1.31.1.1.1.18");
    const size_t baseSize = kIfAlias.size();

    std::map<uint32_t, std::string> aliasByIndex;
    for (const auto& vb : ifAliasVarbinds) {
        const auto& comps = vb.name.components();
        if (comps.size() != baseSize + 1) {
            continue; // not a plain scalar-per-row cell; ignore defensively
        }
        aliasByIndex[comps[baseSize]] = asStringOrEmpty(vb.value);
    }

    for (IfEntry& entry : entries) {
        auto it = aliasByIndex.find(entry.ifIndex);
        if (it != aliasByIndex.end()) {
            entry.ifAlias = it->second;
        }
    }
}

DevicePollResult pollIfTable(SnmpClient& client) {
    static const Oid kIfEntryBase = Oid::parse("1.3.6.1.2.1.2.2.1");
    static const Oid kIfAlias = Oid::parse("1.3.6.1.2.1.31.1.1.1.18");
    static const Oid kSysUpTime = Oid::parse("1.3.6.1.2.1.1.3.0");
    static const Oid kSysDescr = Oid::parse("1.3.6.1.2.1.1.1.0");
    static const Oid kBasePortIfIndex = Oid::parse("1.3.6.1.2.1.17.1.4.1.2");
    static const Oid kFdbEntry = Oid::parse("1.3.6.1.2.1.17.4.3.1");

    DevicePollResult result;
    auto start = std::chrono::steady_clock::now();
    result.polledAtUnixSec =
        std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch())
            .count();
    try {
        std::vector<VarBind> varbinds = client.walkSubtree(kIfEntryBase);
        result.interfaces = bucketIfTableVarBinds(varbinds);

        // Best-effort: ifXTable is an RFC2863 extension, not every agent
        // implements it. A failed/empty walk just leaves ifAlias blank
        // rather than failing the whole poll.
        try {
            std::vector<VarBind> aliasVarbinds = client.walkSubtree(kIfAlias);
            mergeIfAlias(result.interfaces, aliasVarbinds);
        } catch (const SnmpTimeoutError&) {
            // ifXTable unsupported or unreachable; leave ifAlias empty.
        }

        // Best-effort, same reasoning as ifAlias above: BRIDGE-MIB isn't
        // universally implemented, so a failed/empty walk just leaves
        // macTable empty rather than failing the whole poll.
        try {
            std::vector<VarBind> portIndexVarbinds = client.walkSubtree(kBasePortIfIndex);
            auto portMap = bucketBridgePortIfIndex(portIndexVarbinds);
            std::vector<VarBind> fdbVarbinds = client.walkSubtree(kFdbEntry);
            result.macTable = bucketMacTable(fdbVarbinds, portMap);
        } catch (const SnmpTimeoutError&) {
            // BRIDGE-MIB unsupported or unreachable; leave macTable empty.
        }

        // One request for both scalars: SNMP GET responses mirror the
        // request's varbind order, so varBinds[0]/[1] line up with
        // kSysUpTime/kSysDescr without needing to match by OID.
        SnmpGetResult sysInfo = client.get({kSysUpTime, kSysDescr});
        if (sysInfo.errorStatus == 0 && sysInfo.varBinds.size() == 2) {
            if (sysInfo.varBinds[0].value.tag == ber::Tag::TimeTicks) {
                result.sysUpTimeTicks = static_cast<uint32_t>(sysInfo.varBinds[0].value.asUint());
            }
            if (sysInfo.varBinds[1].value.tag == ber::Tag::OctetString) {
                result.sysDescr = sysInfo.varBinds[1].value.asOctetString();
            }
        }

        result.reachable = true;
    } catch (const SnmpTimeoutError& e) {
        result.reachable = false;
        result.error = e.what();
    }
    result.scrapeDurationMs = static_cast<uint32_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count());

    return result;
}

} // namespace wiresprite
