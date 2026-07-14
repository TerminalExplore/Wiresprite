#include "poll/mac_table.hpp"

#include <map>

#include "snmp/oid.hpp"

namespace wiresprite {

namespace {

constexpr int32_t kFdbStatusLearned = 3;

int32_t asIntOrZero(const ber::Value& v) {
    return v.tag == ber::Tag::Integer ? static_cast<int32_t>(v.asInt()) : 0;
}

struct PendingFdbEntry {
    uint32_t port = 0;
    int32_t status = 0;
};

} // namespace

std::unordered_map<uint32_t, uint32_t> bucketBridgePortIfIndex(const std::vector<VarBind>& varbinds) {
    static const Oid kBasePortIfIndex = Oid::parse("1.3.6.1.2.1.17.1.4.1.2");
    const size_t baseSize = kBasePortIfIndex.size();

    std::unordered_map<uint32_t, uint32_t> portToIfIndex;
    for (const auto& vb : varbinds) {
        const auto& comps = vb.name.components();
        if (comps.size() != baseSize + 1) {
            continue; // not a plain scalar-per-row cell; ignore defensively
        }
        uint32_t bridgePort = comps[baseSize];
        portToIfIndex[bridgePort] = static_cast<uint32_t>(asIntOrZero(vb.value));
    }
    return portToIfIndex;
}

std::vector<MacEntry> bucketMacTable(const std::vector<VarBind>& varbinds,
                                      const std::unordered_map<uint32_t, uint32_t>& portMap) {
    static const Oid kFdbEntry = Oid::parse("1.3.6.1.2.1.17.4.3.1");
    const size_t baseSize = kFdbEntry.size();

    // dot1dTpFdbAddress (column 1) just repeats the MAC the index
    // already carries, so only port (column 2) and status (column 3)
    // are tracked here.
    std::map<std::array<uint8_t, 6>, PendingFdbEntry> byAddress;

    for (const auto& vb : varbinds) {
        const auto& comps = vb.name.components();
        if (comps.size() != baseSize + 7) {
            continue; // not column + 6-byte MAC index; ignore defensively
        }
        uint32_t column = comps[baseSize];
        std::array<uint8_t, 6> address{};
        for (size_t i = 0; i < 6; ++i) {
            address[i] = static_cast<uint8_t>(comps[baseSize + 1 + i]);
        }

        PendingFdbEntry& entry = byAddress[address];
        if (column == 2) {
            entry.port = static_cast<uint32_t>(asIntOrZero(vb.value));
        } else if (column == 3) {
            entry.status = asIntOrZero(vb.value);
        }
    }

    std::vector<MacEntry> result;
    for (const auto& [address, entry] : byAddress) {
        if (entry.status != kFdbStatusLearned) {
            continue; // self/mgmt/other/invalid: not "what's plugged in"
        }
        auto ifIndexIt = portMap.find(entry.port);
        if (ifIndexIt == portMap.end()) {
            continue; // can't attribute this MAC to a known interface
        }
        MacEntry mac;
        mac.address = address;
        mac.ifIndex = ifIndexIt->second;
        result.push_back(mac);
    }
    return result;
}

} // namespace wiresprite
